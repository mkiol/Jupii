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
#include "libupnpp/control/avtransport.hxx"

#include <stdlib.h>                     // for atoi
#include <upnp/upnp.h>                  // for UPNP_E_SUCCESS, etc

#include <ostream>                      // for basic_ostream, endl, etc
#include <string>                       // for string, basic_string, etc
#include <utility>                      // for pair
#include <vector>                       // for vector

#include "libupnpp/control/avlastchg.hxx"  // for decodeAVLastChange
#include "libupnpp/control/cdircontent.hxx"  // for UPnPDirContent, etc
#include "libupnpp/log.hxx"             // for LOGERR, LOGDEB1, LOGDEB, etc
#include "libupnpp/soaphelp.hxx"        // for SoapIncoming, etc
#include "libupnpp/upnpavutils.hxx"     // for upnpdurationtos, etc
#include "libupnpp/upnpp_p.hxx"
#include "libupnpp/smallut.h" 

using namespace std;
using namespace std::placeholders;
using namespace UPnPP;

namespace UPnPClient {

const string AVTransport::SType("urn:schemas-upnp-org:service:AVTransport:1");

// Check serviceType string (while walking the descriptions. We don't
// include a version in comparisons, as we are satisfied with version1
bool AVTransport::isAVTService(const string& st)
{
    const string::size_type sz(SType.size()-2);
    return !SType.compare(0, sz, st, 0, sz);
}

static AVTransport::TransportState stringToTpState(const string& s)
{
    if (!stringuppercmp("STOPPED", s)) {
        return AVTransport::Stopped;
    } else if (!stringuppercmp("PLAYING", s)) {
        return AVTransport::Playing;
    } else if (!stringuppercmp("TRANSITIONING", s)) {
        return AVTransport::Transitioning;
    } else if (!stringuppercmp("PAUSED_PLAYBACK", s)) {
        return AVTransport::PausedPlayback;
    } else if (!stringuppercmp("PAUSED_RECORDING", s)) {
        return AVTransport::PausedRecording;
    } else if (!stringuppercmp("RECORDING", s)) {
        return AVTransport::Recording;
    } else if (!stringuppercmp("NO_MEDIA_PRESENT", s)) {
        return AVTransport::NoMediaPresent;
    } else {
        LOGINF("AVTransport event: bad value for TransportState: "
               << s << endl);
        return AVTransport::Unknown;
    }
}

static AVTransport::TransportStatus stringToTpStatus(const string& s)
{
    if (!stringuppercmp("OK", s)) {
        return AVTransport::TPS_Ok;
    } else if (!stringuppercmp("ERROR_OCCURRED", s)) {
        return  AVTransport::TPS_Error;
    } else {
        LOGERR("AVTransport event: bad value for TransportStatus: "
               << s << endl);
        return  AVTransport::TPS_Unknown;
    }
}

static AVTransport::PlayMode stringToPlayMode(const string& s)
{
    if (!stringuppercmp("NORMAL", s)) {
        return AVTransport::PM_Normal;
    } else if (!stringuppercmp("SHUFFLE", s)) {
        return AVTransport::PM_Shuffle;
    } else if (!stringuppercmp("REPEAT_ONE", s)) {
        return AVTransport::PM_RepeatOne;
    } else if (!stringuppercmp("REPEAT_ALL", s)) {
        return AVTransport::PM_RepeatAll;
    } else if (!stringuppercmp("RANDOM", s)) {
        return AVTransport::PM_Random;
    } else if (!stringuppercmp("DIRECT_1", s)) {
        return AVTransport::PM_Direct1;
    } else {
        LOGERR("AVTransport event: bad value for PlayMode: "
               << s << endl);
        return AVTransport::PM_Unknown;
    }
}

void AVTransport::evtCallback(
    const std::unordered_map<std::string, std::string>& props)
{
    LOGDEB1("AVTransport::evtCallback:" << endl);
    for (std::unordered_map<std::string, std::string>::const_iterator it =
                props.begin(); it != props.end(); it++) {
        if (it->first.compare("LastChange")) {
            LOGINF("AVTransport:event: var not lastchange: "
                   << it->first << " -> " << it->second << endl);
            continue;
        }
        LOGDEB1("AVTransport:event: "
                << it->first << " -> " << it->second << endl);

        std::unordered_map<std::string, std::string> props1;
        if (!decodeAVLastChange(it->second, props1)) {
            LOGERR("AVTransport::evtCallback: bad LastChange value: "
                   << it->second << endl);
            return;
        }
        for (std::unordered_map<std::string, std::string>::iterator it1 =
                    props1.begin(); it1 != props1.end(); it1++) {
            if (!getReporter()) {
                LOGDEB1("AVTransport::evtCallback: " << it1->first << " -> "
                        << it1->second << endl);
                continue;
            }

            if (!it1->first.compare("TransportState")) {
                getReporter()->changed(it1->first.c_str(),
                                       stringToTpState(it1->second));

            } else if (!it1->first.compare("TransportStatus")) {
                getReporter()->changed(it1->first.c_str(),
                                       stringToTpStatus(it1->second));

            } else if (!it1->first.compare("CurrentPlayMode")) {
                getReporter()->changed(it1->first.c_str(),
                                       stringToPlayMode(it1->second));

            } else if (!it1->first.compare("CurrentTransportActions")) {
                int iacts;
                if (!CTAStringToBits(it1->second, iacts))
                    getReporter()->changed(it1->first.c_str(), iacts);

            } else if (!it1->first.compare("CurrentTrackURI") ||
                       !it1->first.compare("AVTransportURI") ||
                       !it1->first.compare("NextAVTransportURI")) {
                getReporter()->changed(it1->first.c_str(),
                                       it1->second.c_str());

            } else if (!it1->first.compare("TransportPlaySpeed") ||
                       !it1->first.compare("CurrentTrack") ||
                       !it1->first.compare("NumberOfTracks") ||
                       !it1->first.compare("RelativeCounterPosition") ||
                       !it1->first.compare("AbsoluteCounterPosition") ||
                       !it1->first.compare("InstanceID")) {
                getReporter()->changed(it1->first.c_str(),
                                       atoi(it1->second.c_str()));

            } else if (!it1->first.compare("CurrentMediaDuration") ||
                       !it1->first.compare("CurrentTrackDuration") ||
                       !it1->first.compare("RelativeTimePosition") ||
                       !it1->first.compare("AbsoluteTimePosition")) {
                getReporter()->changed(it1->first.c_str(),
                                       upnpdurationtos(it1->second));

            } else if (!it1->first.compare("AVTransportURIMetaData") ||
                       !it1->first.compare("NextAVTransportURIMetaData") ||
                       !it1->first.compare("CurrentTrackMetaData")) {
                UPnPDirContent meta;
                if (!meta.parse(it1->second)) {
                    LOGERR("AVTransport event: bad metadata: [" <<
                           it1->second << "]" << endl);
                } else {
                    LOGDEB1("AVTransport event: good metadata: [" <<
                            it1->second << "]" << endl);
                    if (meta.m_items.size() > 0) {
                        getReporter()->changed(it1->first.c_str(),
                                               meta.m_items[0]);
                    }
                }
            } else if (!it1->first.compare("PlaybackStorageMedium") ||
                       !it1->first.compare("PossiblePlaybackStorageMedia") ||
                       !it1->first.compare("RecordStorageMedium") ||
                       !it1->first.compare("PossibleRecordStorageMedia") ||
                       !it1->first.compare("RecordMediumWriteStatus") ||
                       !it1->first.compare("CurrentRecordQualityMode") ||
                       !it1->first.compare("PossibleRecordQualityModes")) {
                getReporter()->changed(it1->first.c_str(),it1->second.c_str());

            } else {
                LOGDEB1("AVTransport event: unknown variable: name [" <<
                        it1->first << "] value [" << it1->second << endl);
                getReporter()->changed(it1->first.c_str(),it1->second.c_str());
            }
        }
    }
}


int AVTransport::setURI(const string& uri, const string& metadata,
                        int instanceID, bool next)
{
    SoapOutgoing args(getServiceType(), next ? "SetNextAVTransportURI" :
                      "SetAVTransportURI");
    args("InstanceID", SoapHelp::i2s(instanceID))
    (next ? "NextURI" : "CurrentURI", uri)
    (next ? "NextURIMetaData" : "CurrentURIMetaData", metadata);

    SoapIncoming data;
    return runAction(args, data);
}

int AVTransport::setPlayMode(PlayMode pm, int instanceID)
{
    SoapOutgoing args(getServiceType(), "SetPlayMode");
    string playmode;
    switch (pm) {
    case PM_Normal:
        playmode = "NORMAL";
        break;
    case PM_Shuffle:
        playmode = "SHUFFLE";
        break;
    case PM_RepeatOne:
        playmode = "REPEAT_ONE";
        break;
    case PM_RepeatAll:
        playmode = "REPEAT_ALL";
        break;
    case PM_Random:
        playmode = "RANDOM";
        break;
    case PM_Direct1:
        playmode = "DIRECT_1";
        break;
    default:
        playmode = "NORMAL";
        break;
    }

    args("InstanceID", SoapHelp::i2s(instanceID))
    ("NewPlayMode", playmode);

    SoapIncoming data;
    return runAction(args, data);
}

int AVTransport::getMediaInfo(MediaInfo& info, int instanceID)
{
    SoapOutgoing args(getServiceType(), "GetMediaInfo");
    args("InstanceID", SoapHelp::i2s(instanceID));
    SoapIncoming data;
    int ret = runAction(args, data);
    if (ret != UPNP_E_SUCCESS) {
        return ret;
    }
    string s;
    data.get("NrTracks", &info.nrtracks);
    data.get("MediaDuration", &s);
    info.mduration = upnpdurationtos(s);
    data.get("CurrentURI", &info.cururi);
    data.get("CurrentURIMetaData", &s);
    UPnPDirContent meta;
    meta.parse(s);
    if (meta.m_items.size() > 0)
        info.curmeta = meta.m_items[0];
    meta.clear();
    data.get("NextURI", &info.nexturi);
    data.get("NextURIMetaData", &s);
    if (meta.m_items.size() > 0)
        info.nextmeta = meta.m_items[0];
    data.get("PlayMedium", &info.pbstoragemed);
    data.get("RecordMedium", &info.pbstoragemed);
    data.get("WriteStatus", &info.ws);
    return 0;
}

int AVTransport::getTransportInfo(TransportInfo& info, int instanceID)
{
    SoapOutgoing args(getServiceType(), "GetTransportInfo");
    args("InstanceID", SoapHelp::i2s(instanceID));
    SoapIncoming data;
    int ret = runAction(args, data);
    if (ret != UPNP_E_SUCCESS) {
        return ret;
    }
    string s;
    data.get("CurrentTransportState", &s);
    info.tpstate = stringToTpState(s);
    data.get("CurrentTransportStatus", &s);
    info.tpstatus = stringToTpStatus(s);
    data.get("CurrentSpeed", &info.curspeed);
    return 0;
}

int AVTransport::getPositionInfo(PositionInfo& info, int instanceID)
{
    SoapOutgoing args(getServiceType(), "GetPositionInfo");
    args("InstanceID", SoapHelp::i2s(instanceID));
    SoapIncoming data;
    int ret = runAction(args, data);
    if (ret != UPNP_E_SUCCESS) {
        return ret;
    }
    string s;
    data.get("Track", &info.track);
    data.get("TrackDuration", &s);
    info.trackduration = upnpdurationtos(s);
    data.get("TrackMetaData", &s);
    UPnPDirContent meta;
    meta.parse(s);
    if (meta.m_items.size() > 0) {
        info.trackmeta = meta.m_items[0];
        LOGDEB1("AVTransport::getPositionInfo: size " <<
                meta.m_items.size() << " current title: "
                << meta.m_items[0].m_title << endl);
    }
    data.get("TrackURI", &info.trackuri);
    data.get("RelTime", &s);
    info.reltime = upnpdurationtos(s);
    data.get("AbsTime", &s);
    info.abstime = upnpdurationtos(s);
    data.get("RelCount", &info.relcount);
    data.get("AbsCount", &info.abscount);
    return 0;
}

int AVTransport::getDeviceCapabilities(DeviceCapabilities& info, int iID)
{
    SoapOutgoing args(getServiceType(), "GetDeviceCapabilities");
    args("InstanceID", SoapHelp::i2s(iID));
    SoapIncoming data;
    int ret = runAction(args, data);
    if (ret != UPNP_E_SUCCESS) {
        return ret;
    }
    data.get("PlayMedia", &info.playmedia);
    data.get("RecMedia", &info.recmedia);
    data.get("RecQualityModes", &info.recqualitymodes);
    return 0;
}

int AVTransport::getTransportSettings(TransportSettings& info, int instanceID)
{
    SoapOutgoing args(getServiceType(), "GetTransportSettings");
    args("InstanceID", SoapHelp::i2s(instanceID));
    SoapIncoming data;
    int ret = runAction(args, data);
    if (ret != UPNP_E_SUCCESS) {
        return ret;
    }
    string s;
    data.get("PlayMedia", &s);
    info.playmode = stringToPlayMode(s);
    data.get("RecQualityMode", &info.recqualitymode);
    return 0;
}

int AVTransport::getCurrentTransportActions(int& iacts, int iID)
{
    SoapOutgoing args(getServiceType(), "GetCurrentTransportActions");
    args("InstanceID", SoapHelp::i2s(iID));
    SoapIncoming data;
    int ret = runAction(args, data);
    if (ret != UPNP_E_SUCCESS) {
        return ret;
    }
    string actions;
    if (!data.get("Actions", &actions)) {
        LOGERR("AVTransport:getCurrentTransportActions: no actions in answer"
               << endl);
        return UPNP_E_BAD_RESPONSE;
    }
    return CTAStringToBits(actions, iacts);
}

int AVTransport::CTAStringToBits(const string& actions, int& iacts)
{
    vector<string> sacts;
    if (!csvToStrings(actions, sacts)) {
        LOGERR("AVTransport::CTAStringToBits: bad actions string:"
               << actions << endl);
        return UPNP_E_BAD_RESPONSE;
    }
    iacts = 0;
    for (vector<string>::iterator it = sacts.begin(); it != sacts.end(); it++) {
        trimstring(*it);
        if (!it->compare("Next")) {
            iacts |= TPA_Next;
        } else if (!it->compare("Pause")) {
            iacts |= TPA_Pause;
        } else if (!it->compare("Play")) {
            iacts |= TPA_Play;
        } else if (!it->compare("Previous")) {
            iacts |= TPA_Previous;
        } else if (!it->compare("Seek")) {
            iacts |= TPA_Seek;
        } else if (!it->compare("Stop")) {
            iacts |= TPA_Stop;
        } else if (it->empty()) {
            continue;
        } else {
            LOGERR("AVTransport::CTAStringToBits: unknown action in " <<
                   actions << " : [" << *it << "]" << endl);
        }
    }
    return 0;
}

int AVTransport::stop(int instanceID)
{
    SoapOutgoing args(getServiceType(), "Stop");
    args("InstanceID", SoapHelp::i2s(instanceID));
    SoapIncoming data;
    return runAction(args, data);
}

int AVTransport::pause(int instanceID)
{
    SoapOutgoing args(getServiceType(), "Pause");
    args("InstanceID", SoapHelp::i2s(instanceID));
    SoapIncoming data;
    return runAction(args, data);
}

int AVTransport::play(int speed, int instanceID)
{
    SoapOutgoing args(getServiceType(), "Play");
    args("InstanceID", SoapHelp::i2s(instanceID))
    ("Speed", SoapHelp::i2s(speed));
    SoapIncoming data;
    return runAction(args, data);
}

int AVTransport::seek(SeekMode mode, int target, int instanceID)
{
    string sm;
    string value = SoapHelp::i2s(target);
    switch (mode) {
    case SEEK_TRACK_NR:
        sm = "TRACK_NR";
        break;
    case SEEK_ABS_TIME:
        sm = "ABS_TIME";
        value = upnpduration(target*1000);
        break;
    case SEEK_REL_TIME:
        sm = "REL_TIME";
        value = upnpduration(target*1000);
        break;
    case SEEK_ABS_COUNT:
        sm = "ABS_COUNT";
        break;
    case SEEK_REL_COUNT:
        sm = "REL_COUNT";
        break;
    case SEEK_CHANNEL_FREQ:
        sm = "CHANNEL_FREQ";
        break;
    case SEEK_TAPE_INDEX:
        sm = "TAPE-INDEX";
        break;
    case SEEK_FRAME:
        sm = "FRAME";
        break;
    default:
        return UPNP_E_INVALID_PARAM;
    }

    SoapOutgoing args(getServiceType(), "Seek");
    args("InstanceID", SoapHelp::i2s(instanceID))
    ("Unit", sm)
    ("Target", value);
    SoapIncoming data;
    return runAction(args, data);
}

int AVTransport::next(int instanceID)
{
    SoapOutgoing args(getServiceType(), "Next");
    args("InstanceID", SoapHelp::i2s(instanceID));
    SoapIncoming data;
    return runAction(args, data);
}

int AVTransport::previous(int instanceID)
{
    SoapOutgoing args(getServiceType(), "Previous");
    args("InstanceID", SoapHelp::i2s(instanceID));
    SoapIncoming data;
    return runAction(args, data);
}

void AVTransport::registerCallback()
{
    Service::registerCallback(bind(&AVTransport::evtCallback, this, _1));
}

} // End namespace UPnPClient
