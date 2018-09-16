/* Copyright (C) 2006-2016 J.F.Dockes
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 *   02110-1301 USA
 */
#include "libupnpp/control/ohplaylist.hxx"

#include <stdlib.h>                     // for atoi
#include <string.h>                     // for strcmp
#include <upnp/upnp.h>                  // for UPNP_E_BAD_RESPONSE, etc

#include <functional>                   // for _Bind, bind, _1
#include <ostream>                      // for endl, basic_ostream, etc
#include <string>                       // for string, basic_string, etc
#include <utility>                      // for pair
#include <vector>                       // for vector

#include "libupnpp/upnpavutils.hxx"
#include "libupnpp/control/cdircontent.hxx"  // for UPnPDirContent, etc
#include "libupnpp/control/service.hxx"  // for VarEventReporter, Service
#include "libupnpp/expatmm.hxx"         // for inputRefXMLParser
#include "libupnpp/log.hxx"             // for LOGERR, LOGDEB1, LOGINF
#include "libupnpp/soaphelp.hxx"        // for SoapIncoming, etc
#include "libupnpp/upnpp_p.hxx"         // for stringToBool

using namespace std;
using namespace std::placeholders;
using namespace UPnPP;

namespace UPnPClient {

const string OHPlaylist::SType("urn:av-openhome-org:service:Playlist:1");

// Check serviceType string (while walking the descriptions. We don't
// include a version in comparisons, as we are satisfied with version1
bool OHPlaylist::isOHPlService(const string& st)
{
    const string::size_type sz(SType.size()-2);
    return !SType.compare(0, sz, st, 0, sz);
}

int OHPlaylist::stringToTpState(const string& value, OHPlaylist::TPState *tpp)
{
    if (!value.compare("Buffering") || !value.compare("Waiting")) {
        *tpp = OHPlaylist::TPS_Buffering;
        return 0;
    } else if (!value.compare("Paused")) {
        *tpp = OHPlaylist::TPS_Paused;
        return 0;
    } else if (!value.compare("Playing")) {
        *tpp = OHPlaylist::TPS_Playing;
        return 0;
    } else if (!value.compare("Stopped")) {
        *tpp = OHPlaylist::TPS_Stopped;
        return 0;
    }
    *tpp = OHPlaylist::TPS_Unknown;
    return UPNP_E_BAD_RESPONSE;
}

void OHPlaylist::evtCallback(
    const std::unordered_map<std::string, std::string>& props)
{
    LOGDEB1("OHPlaylist::evtCallback: getReporter(): " << getReporter() << endl);
    for (std::unordered_map<std::string, std::string>::const_iterator it =
                props.begin(); it != props.end(); it++) {
        if (!getReporter()) {
            LOGDEB1("OHPlaylist::evtCallback: " << it->first << " -> "
                    << it->second << endl);
            continue;
        }

        if (!it->first.compare("TransportState")) {
            TPState tp;
            stringToTpState(it->second, &tp);
            getReporter()->changed(it->first.c_str(), int(tp));

        } else if (!it->first.compare("ProtocolInfo")) {
            getReporter()->changed(it->first.c_str(),
                                   it->second.c_str());

        } else if (!it->first.compare("Repeat") ||
                   !it->first.compare("Shuffle")) {
            bool val = false;
            stringToBool(it->second, &val);
            getReporter()->changed(it->first.c_str(), val ? 1 : 0);

        } else if (!it->first.compare("Id") ||
                   !it->first.compare("TracksMax")) {
            getReporter()->changed(it->first.c_str(),
                                   atoi(it->second.c_str()));

        } else if (!it->first.compare("IdArray")) {
            // Decode IdArray. See how we call the client
            vector<int> v;
            ohplIdArrayToVec(it->second, &v);
            getReporter()->changed(it->first.c_str(), v);

        } else {
            LOGERR("OHPlaylist event: unknown variable: name [" <<
                   it->first << "] value [" << it->second << endl);
            getReporter()->changed(it->first.c_str(), it->second.c_str());
        }
    }
}

void OHPlaylist::registerCallback()
{
    Service::registerCallback(bind(&OHPlaylist::evtCallback, this, _1));
}

int OHPlaylist::play()
{
    return runTrivialAction("Play");
}
int OHPlaylist::pause()
{
    return runTrivialAction("Pause");
}
int OHPlaylist::stop()
{
    return runTrivialAction("Stop");
}
int OHPlaylist::next()
{
    return runTrivialAction("Next");
}
int OHPlaylist::previous()
{
    return runTrivialAction("Previous");
}
int OHPlaylist::setRepeat(bool onoff)
{
    return runSimpleAction("SetRepeat", "Value", onoff);
}
int OHPlaylist::repeat(bool *on)
{
    return runSimpleGet("Repeat", "Value", on);
}
int OHPlaylist::setShuffle(bool onoff)
{
    return runSimpleAction("SetShuffle", "Value", onoff);
}
int OHPlaylist::shuffle(bool *on)
{
    return runSimpleGet("Shuffle", "Value", on);
}
int OHPlaylist::seekSecondAbsolute(int value)
{
    return runSimpleAction("SeekSecondAbsolute", "Value", value);
}
int OHPlaylist::seekSecondRelative(int value)
{
    return runSimpleAction("SeekSecondRelative", "Value", value);
}
int OHPlaylist::seekId(int value)
{
    return runSimpleAction("SeekId", "Value", value);
}
int OHPlaylist::seekIndex(int value)
{
    return runSimpleAction("SeekIndex", "Value", value);
}

int OHPlaylist::transportState(TPState* tpp)
{
    string value;
    int ret;

    if ((ret = runSimpleGet("TransportState", "Value", &value)))
        return ret;

    return stringToTpState(value, tpp);
}

int OHPlaylist::id(int *value)
{
    return runSimpleGet("Id", "Value", value);
}

int OHPlaylist::read(int id, std::string* urip, UPnPDirObject *dirent)
{
    SoapOutgoing args(getServiceType(), "Read");
    args("Id", SoapHelp::i2s(id));
    SoapIncoming data;
    int ret = runAction(args, data);
    if (ret != UPNP_E_SUCCESS) {
        return ret;
    }
    if (!data.get("Uri", urip)) {
        LOGERR("OHPlaylist::Read: missing Uri in response" << endl);
        return UPNP_E_BAD_RESPONSE;
    }
    string didl;
    if (!data.get("Metadata", &didl)) {
        LOGERR("OHPlaylist::Read: missing Uri in response" << endl);
        return UPNP_E_BAD_RESPONSE;
    }
    didl = SoapHelp::xmlUnquote(didl);

    UPnPDirContent dir;
    if (!dir.parse(didl)) {
        LOGERR("OHPlaylist::Read: didl parse failed: " << didl << endl);
        return UPNP_E_BAD_RESPONSE;
    }
    if (dir.m_items.size() != 1) {
        LOGERR("OHPlaylist::Read: " << dir.m_items.size() << " in response!" <<
               endl);
        return UPNP_E_BAD_RESPONSE;
    }
    *dirent = dir.m_items[0];
    return 0;
}

// Tracklist format
// <TrackList>
//   <Entry>
//     <Id>10</Id>
//     <Uri>http://blabla</Uri>
//     <Metadata>(xmlencoded didl)</Metadata>
//   </Entry>
// </TrackList>


class OHTrackListParser : public inputRefXMLParser {
public:
    OHTrackListParser(const string& input,
                      vector<OHPlaylist::TrackListEntry>* vp)
        : inputRefXMLParser(input), m_v(vp)
    {
        //LOGDEB("OHTrackListParser: input: " << input << endl);
    }

protected:
    virtual void StartElement(const XML_Char *name, const XML_Char **) {
        m_path.push_back(name);
    }
    virtual void EndElement(const XML_Char *name) {
        if (!strcmp(name, "Entry")) {
            UPnPDirContent dir;
            if (!dir.parse(m_tdidl)) {
                LOGERR("OHPlaylist::ReadList: didl parse failed: "
                       << m_tdidl << endl);
                return;
            }
            if (dir.m_items.size() != 1) {
                LOGERR("OHPlaylist::ReadList: " << dir.m_items.size()
                       << " in response!" << endl);
                return;
            }
            m_tt.dirent = dir.m_items[0];
            m_v->push_back(m_tt);
            m_tt.clear();
            m_tdidl.clear();
        }
        m_path.pop_back();
    }
    virtual void CharacterData(const XML_Char *s, int len) {
        if (s == 0 || *s == 0)
            return;
        string str(s, len);
        if (!m_path.back().compare("Id"))
            m_tt.id = atoi(str.c_str());
        else if (!m_path.back().compare("Uri"))
            m_tt.url = str;
        else if (!m_path.back().compare("Metadata"))
            m_tdidl += str;
    }

private:
    vector<OHPlaylist::TrackListEntry>* m_v;
    std::vector<std::string> m_path;
    OHPlaylist::TrackListEntry m_tt;
    string m_tdidl;
};

int OHPlaylist::readList(const std::vector<int>& ids,
                         vector<TrackListEntry>* entsp)
{
    string idsparam;
    for (vector<int>::const_iterator it = ids.begin(); it != ids.end(); it++) {
        idsparam += SoapHelp::i2s(*it) + " ";
    }
    entsp->clear();

    SoapOutgoing args(getServiceType(), "ReadList");
    args("IdList", idsparam);
    SoapIncoming data;
    int ret = runAction(args, data);
    if (ret != UPNP_E_SUCCESS) {
        return ret;
    }
    string xml;
    if (!data.get("TrackList", &xml)) {
        LOGERR("OHPlaylist::readlist: missing TrackList in response" << endl);
        return UPNP_E_BAD_RESPONSE;
    }
    OHTrackListParser mparser(xml, entsp);
    if (!mparser.Parse())
        return UPNP_E_BAD_RESPONSE;
    return 0;
}

int OHPlaylist::insert(int afterid, const string& uri, const string& didl,
                       int *nid)
{
    SoapOutgoing args(getServiceType(), "Insert");
    args("AfterId", SoapHelp::i2s(afterid))
    ("Uri", uri)
    ("Metadata", didl);
    SoapIncoming data;
    int ret = runAction(args, data);
    if (ret != UPNP_E_SUCCESS) {
        return ret;
    }
    int lnid;
    if (!data.get("NewId", &lnid)) {
        LOGERR("OHPlaylist::insert: missing Newid in response" << endl);
        return UPNP_E_BAD_RESPONSE;
    }
    if (nid) {
        *nid = lnid;
    }
    return 0;
}


int OHPlaylist::deleteId(int value)
{
    return runSimpleAction("DeleteId", "Value", value);
}
int OHPlaylist::deleteAll()
{
    return runTrivialAction("DeleteAll");
}
int OHPlaylist::tracksMax(int *valuep)
{
    return runSimpleGet("TracksMax", "Value", valuep);
}

int OHPlaylist::idArray(vector<int> *ids, int *tokp)
{
    SoapOutgoing args(getServiceType(), "IdArray");
    SoapIncoming data;
    int ret = runAction(args, data);
    if (ret != UPNP_E_SUCCESS) {
        return ret;
    }
    int ltok;
    if (!data.get("Token", &ltok)) {
        LOGERR("OHPlaylist::idArray: missing Token in response" << endl);
        return UPNP_E_BAD_RESPONSE;
    }
    if (tokp) {
        *tokp = ltok;
    }
    string arraydata;
    if (!data.get("Array", &arraydata)) {
        LOGINF("OHPlaylist::idArray: missing Array in response" << endl);
        // We get this for an empty array ? This would need to be investigated
    }
    ohplIdArrayToVec(arraydata, ids);
    return 0;
}

int OHPlaylist::idArrayChanged(int token, bool *changed)
{
    SoapOutgoing args(getServiceType(), "IdArrayChanged");
    args("Token", SoapHelp::i2s(token));
    SoapIncoming data;
    int ret = runAction(args, data);
    if (ret != UPNP_E_SUCCESS) {
        return ret;
    }
    if (!data.get("Value", changed)) {
        LOGERR("OHPlaylist::idArrayChanged: missing Value in response" << endl);
        return UPNP_E_BAD_RESPONSE;
    }
    return 0;
}

int OHPlaylist::protocolInfo(std::string *proto)
{
    SoapOutgoing args(getServiceType(), "ProtocolInfo");
    SoapIncoming data;
    int ret = runAction(args, data);
    if (ret != UPNP_E_SUCCESS) {
        return ret;
    }
    if (!data.get("Value", proto)) {
        LOGERR("OHPlaylist::protocolInfo: missing Value in response" << endl);
        return UPNP_E_BAD_RESPONSE;
    }
    return 0;
}

} // End namespace UPnPClient
