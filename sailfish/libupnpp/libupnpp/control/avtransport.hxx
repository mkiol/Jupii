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
#ifndef _AVTRANSPORT_HXX_INCLUDED_
#define _AVTRANSPORT_HXX_INCLUDED_

#include "libupnpp/config.h"

#include <string>                       // for string

#include "libupnpp/control/cdircontent.hxx"  // for UPnPDirObject
#include "libupnpp/control/service.hxx"  // for Service

namespace UPnPClient {
class AVTransport;
}
namespace UPnPClient {
class UPnPDeviceDesc;
}
namespace UPnPClient {
class UPnPServiceDesc;
}

namespace UPnPClient {

typedef std::shared_ptr<AVTransport> AVTH;

/**
 * AVTransport Service client class.
 *
 */
class AVTransport : public Service {
public:

    /** Construct by copying data from device and service objects.
     *
     */
    AVTransport(const UPnPDeviceDesc& device,
                const UPnPServiceDesc& service)
        : Service(device, service) {
        registerCallback();
    }

    AVTransport() {}
    virtual ~AVTransport() { }

    int setAVTransportURI(const std::string& uri, const std::string& metadata,
                          int instanceID=0)
    {
        return setURI(uri, metadata, instanceID, false);
    }

    int setNextAVTransportURI(const std::string& uri, const std::string& md,
                              int instanceID=0)
    {
        return setURI(uri, md, instanceID, true);
    }


    enum PlayMode {PM_Unknown, PM_Normal, PM_Shuffle, PM_RepeatOne,
                   PM_RepeatAll, PM_Random, PM_Direct1
                  };
    int setPlayMode(PlayMode pm, int instanceID=0);

    struct MediaInfo {
        int nrtracks;
        int mduration; // seconds
        std::string cururi;
        UPnPDirObject curmeta;
        std::string nexturi;
        UPnPDirObject nextmeta;
        std::string pbstoragemed;
        std::string rcstoragemed;
        std::string ws;
    };
    int getMediaInfo(MediaInfo& info, int instanceID=0);

    enum TransportState {Unknown, Stopped, Playing, Transitioning,
                         PausedPlayback, PausedRecording, Recording,
                         NoMediaPresent
                        };
    enum TransportStatus {TPS_Unknown, TPS_Ok, TPS_Error};
    struct TransportInfo {
        TransportState tpstate;
        TransportStatus tpstatus;
        int curspeed;
    };
    int getTransportInfo(TransportInfo& info, int instanceID=0);

    struct PositionInfo {
        int track;
        int trackduration; // secs
        UPnPDirObject trackmeta;
        std::string trackuri;
        int reltime;
        int abstime;
        int relcount;
        int abscount;
    };
    int getPositionInfo(PositionInfo& info, int instanceID=0);

    struct DeviceCapabilities {
        std::string playmedia;
        std::string recmedia;
        std::string recqualitymodes;
    };
    int getDeviceCapabilities(DeviceCapabilities& info, int instanceID=0);

    struct TransportSettings {
        PlayMode playmode;
        std::string recqualitymode;
    };
    int getTransportSettings(TransportSettings& info, int instanceID=0);

    int stop(int instanceID=0);
    int pause(int instanceID=0);
    int play(int speed = 1, int instanceID = 0);

    enum SeekMode {SEEK_TRACK_NR, SEEK_ABS_TIME, SEEK_REL_TIME, SEEK_ABS_COUNT,
                   SEEK_REL_COUNT, SEEK_CHANNEL_FREQ, SEEK_TAPE_INDEX,
                   SEEK_FRAME
                  };
    // Target in seconds for times.
    int seek(SeekMode mode, int target, int instanceID=0);

    // These are for multitrack medium like a CD. No meaning for electronic
    // tracks where set(next)AVTransportURI() is used
    int next(int instanceID=0);
    int previous(int instanceID=0);

    enum TransportActions {TPA_Next = 1, TPA_Pause = 2, TPA_Play = 4,
                           TPA_Previous = 8, TPA_Seek = 16, TPA_Stop = 32
                          };
    int getCurrentTransportActions(int& actions, int instanceID=0);

    /** Test service type from discovery message */
    static bool isAVTService(const std::string& st);

protected:
    /* My service type string */
    static const std::string SType;

    int setURI(const std::string& uri, const std::string& metadata,
               int instanceID, bool next);
    int CTAStringToBits(const std::string& actions, int& iacts);

private:
    void evtCallback(const std::unordered_map<std::string, std::string>&);
    void registerCallback();

};

} // namespace UPnPClient

#endif /* _AVTRANSPORT_HXX_INCLUDED_ */
