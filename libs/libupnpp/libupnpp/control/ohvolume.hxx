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
#ifndef _OHVOLUME_HXX_INCLUDED_
#define _OHVOLUME_HXX_INCLUDED_

#include "libupnpp/config.h"

#include <unordered_map>
#include <memory>                       // for shared_ptr
#include <string>                       // for string
#include <vector>                       // for vector

#include "service.hxx"                  // for Service

namespace UPnPClient {
class OHVolume;
}
namespace UPnPClient {
class UPnPDeviceDesc;
}
namespace UPnPClient {
class UPnPServiceDesc;
}

namespace UPnPClient {

typedef std::shared_ptr<OHVolume> OHVLH;

/**
 * OHVolume Service client class.
 *
 */
class OHVolume : public Service {
public:

    OHVolume(const UPnPDeviceDesc& device, const UPnPServiceDesc& service)
        : Service(device, service), m_volmax(-1) {
        registerCallback();
    }
    virtual ~OHVolume() {
    }

    OHVolume() {}

    /** Test service type from discovery message */
    static bool isOHVLService(const std::string& st);

    int volume(int *value);
    int setVolume(int value);
    int volumeLimit(int *value);
    int mute(bool *value);
    int setMute(bool value);

protected:
    /* My service type string */
    static const std::string SType;

private:
    void evtCallback(const std::unordered_map<std::string, std::string>&);
    void registerCallback();
    int devVolTo0100(int);
    int vol0100ToDev(int vol);
    int maybeInitVolmax();

    int m_volmax;
};

} // namespace UPnPClient

#endif /* _OHVOLUME_HXX_INCLUDED_ */
