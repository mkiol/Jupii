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
#include "libupnpp/control/ohradio.hxx"

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

const string OHRadio::SType("urn:av-openhome-org:service:Radio:1");

// Check serviceType string (while walking the descriptions. We don't
// include a version in comparisons, as we are satisfied with version1
bool OHRadio::isOHRdService(const string& st)
{
    const string::size_type sz(SType.size()-2);
    return !SType.compare(0, sz, st, 0, sz);
}

int OHRadio::decodeMetadata(const string& who,
                            const string &didl, UPnPDirObject *dirent)
{
    UPnPDirContent dir;
    if (!dir.parse(didl)) {
        LOGERR("OHRadio::decodeMetadata: " << who << ": didl parse failed: "
               << didl << endl);
        return UPNP_E_BAD_RESPONSE;
    }
    if (dir.m_items.size() != 1) {
        LOGERR("OHRadio::decodeMetadata: " << who << ": " << dir.m_items.size()
               << " items in response: [" << didl << "]" << endl);
        return UPNP_E_BAD_RESPONSE;
    }
    *dirent = dir.m_items[0];
    return 0;
}

void OHRadio::evtCallback(
    const std::unordered_map<std::string, std::string>& props)
{
    LOGDEB1("OHRadio::evtCallback: getReporter(): " << getReporter() << endl);
    for (std::unordered_map<std::string, std::string>::const_iterator it =
                props.begin(); it != props.end(); it++) {
        if (!getReporter()) {
            LOGDEB1("OHRadio::evtCallback: " << it->first << " -> "
                    << it->second << endl);
            continue;
        }

        if (!it->first.compare("Id") ||
                !it->first.compare("ChannelsMax")) {
            getReporter()->changed(it->first.c_str(), atoi(it->second.c_str()));
        } else if (!it->first.compare("IdArray")) {
            // Decode IdArray. See how we call the client
            vector<int> v;
            ohplIdArrayToVec(it->second, &v);
            getReporter()->changed(it->first.c_str(), v);
        } else if (!it->first.compare("ProtocolInfo") ||
                   !it->first.compare("Uri")) {
            getReporter()->changed(it->first.c_str(), it->second.c_str());
        } else if (!it->first.compare("Metadata")) {
            /* Metadata is a didl-lite string */
            UPnPDirObject dirent;
            if (decodeMetadata("evt", it->second, &dirent) == 0) {
                getReporter()->changed(it->first.c_str(), dirent);
            } else {
                LOGDEB("OHRadio:evtCallback: bad metadata in event\n");
            }
        } else if (!it->first.compare("TransportState")) {
            OHPlaylist::TPState tp;
            OHPlaylist::stringToTpState(it->second, &tp);
            getReporter()->changed(it->first.c_str(), int(tp));
        } else {
            LOGERR("OHRadio event: unknown variable: name [" <<
                   it->first << "] value [" << it->second << endl);
            getReporter()->changed(it->first.c_str(), it->second.c_str());
        }
    }
}

void OHRadio::registerCallback()
{
    Service::registerCallback(bind(&OHRadio::evtCallback, this, _1));
}

int OHRadio::channel(std::string* urip, UPnPDirObject *dirent)
{
    SoapOutgoing args(getServiceType(), "Channel");
    SoapIncoming data;
    int ret = runAction(args, data);
    if (ret != UPNP_E_SUCCESS) {
        return ret;
    }
    if (!data.get("Uri", urip)) {
        LOGERR("OHRadio::Read: missing Uri in response" << endl);
        return UPNP_E_BAD_RESPONSE;
    }
    string didl;
    if (!data.get("Metadata", &didl)) {
        LOGERR("OHRadio::Read: missing Uri in response" << endl);
        return UPNP_E_BAD_RESPONSE;
    }
    return decodeMetadata("channel", didl, dirent);
}

int OHRadio::channelsMax(int *value)
{
    return runSimpleGet("ChannelsMax", "Value", value);
}

int OHRadio::id(int *value)
{
    return runSimpleGet("Id", "Value", value);
}

int OHRadio::idArray(vector<int> *ids, int *tokp)
{
    SoapOutgoing args(getServiceType(), "IdArray");
    SoapIncoming data;
    int ret = runAction(args, data);
    if (ret != UPNP_E_SUCCESS) {
        return ret;
    }
    if (!data.get("Token", tokp)) {
        LOGERR("OHRadio::idArray: missing Token in response" << endl);
        return UPNP_E_BAD_RESPONSE;
    }
    string arraydata;
    if (!data.get("Array", &arraydata)) {
        LOGINF("OHRadio::idArray: missing Array in response" << endl);
        // We get this for an empty array ? This would need to be investigated
    }
    ohplIdArrayToVec(arraydata, ids);
    return 0;
}

int OHRadio::idArrayChanged(int token, bool *changed)
{
    SoapOutgoing args(getServiceType(), "IdArrayChanged");
    args("Token", SoapHelp::i2s(token));
    SoapIncoming data;
    int ret = runAction(args, data);
    if (ret != UPNP_E_SUCCESS) {
        return ret;
    }
    if (!data.get("Value", changed)) {
        LOGERR("OHRadio::idArrayChanged: missing Value in response" << endl);
        return UPNP_E_BAD_RESPONSE;
    }
    return 0;
}

int OHRadio::pause()
{
    return runTrivialAction("Pause");
}

int OHRadio::play()
{
    return runTrivialAction("Play");
}

int OHRadio::protocolInfo(std::string *proto)
{
    SoapOutgoing args(getServiceType(), "ProtocolInfo");
    SoapIncoming data;
    int ret = runAction(args, data);
    if (ret != UPNP_E_SUCCESS) {
        return ret;
    }
    if (!data.get("Value", proto)) {
        LOGERR("OHRadio::protocolInfo: missing Value in response" << endl);
        return UPNP_E_BAD_RESPONSE;
    }
    return 0;
}

int OHRadio::read(int id, UPnPDirObject *dirent)
{
    SoapOutgoing args(getServiceType(), "Read");
    args("Id", SoapHelp::i2s(id));
    SoapIncoming data;
    int ret = runAction(args, data);
    if (ret != UPNP_E_SUCCESS) {
        return ret;
    }
    string didl;
    if (!data.get("Metadata", &didl)) {
        LOGERR("OHRadio::Read: missing Metadata in response" << endl);
        return UPNP_E_BAD_RESPONSE;
    }
    return decodeMetadata("read", didl, dirent);
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
                LOGERR("OHRadio::ReadList: didl parse failed: "
                       << m_tdidl << endl);
                return;
            }
            if (dir.m_items.size() != 1) {
                LOGERR("OHRadio::ReadList: " << dir.m_items.size()
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

int OHRadio::readList(const std::vector<int>& ids,
                      vector<OHPlaylist::TrackListEntry>* entsp)
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
    if (!data.get("ChannelList", &xml)) {
        LOGERR("OHRadio::readlist: missing TrackList in response" << endl);
        return UPNP_E_BAD_RESPONSE;
    }
    OHTrackListParser mparser(xml, entsp);
    if (!mparser.Parse())
        return UPNP_E_BAD_RESPONSE;
    return 0;
}

int OHRadio::setChannel(const std::string& uri, const std::string& didl)
{
    SoapOutgoing args(getServiceType(), "SetChannel");
    args("Uri", uri)
    ("Metadata", didl);
    SoapIncoming data;
    return runAction(args, data);
}

int OHRadio::setId(int id, const std::string& uri)
{
    SoapOutgoing args(getServiceType(), "SetId");
    args("Value", SoapHelp::val2s(id))
    ("Uri", uri);
    SoapIncoming data;
    return runAction(args, data);
}

int OHRadio::stop()
{
    return runTrivialAction("Stop");
}

int OHRadio::transportState(OHPlaylist::TPState* tpp)
{
    string value;
    int ret;

    if ((ret = runSimpleGet("TransportState", "Value", &value)))
        return ret;

    return OHPlaylist::stringToTpState(value, tpp);
}

} // End namespace UPnPClient
