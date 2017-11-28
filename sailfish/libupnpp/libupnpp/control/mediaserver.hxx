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
#ifndef _MEDIASERVER_HXX_INCLUDED_
#define _MEDIASERVER_HXX_INCLUDED_

#include <memory>                       // for shared_ptr
#include <string>                       // for string
#include <vector>                       // for vector

#include "libupnpp/control/cdirectory.hxx"  // for CDSH
#include "libupnpp/control/device.hxx"  // for Device

namespace UPnPClient {

class MediaServer;
class UPnPDeviceDesc;

typedef std::shared_ptr<MediaServer> MSRH;

class MediaServer : public Device {
public:
    MediaServer(const UPnPDeviceDesc& desc);

    CDSH cds() {
        return m_cds;
    }

    static bool getDeviceDescs(std::vector<UPnPDeviceDesc>& devices,
                               const std::string& friendlyName = "");
    static bool isMSDevice(const std::string& devicetype);

protected:
    CDSH m_cds;

    static const std::string DType;
};

}

#endif /* _MEDIASERVER_HXX_INCLUDED_ */
