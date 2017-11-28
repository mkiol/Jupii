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
#ifndef _RENDERINGCONTROL_HXX_INCLUDED_
#define _RENDERINGCONTROL_HXX_INCLUDED_

#include "libupnpp/config.h"

#include <string>                       // for string

#include "service.hxx"                  // for Service

namespace UPnPClient {
class RenderingControl;
}
namespace UPnPClient {
class UPnPDeviceDesc;
}
namespace UPnPClient {
class UPnPServiceDesc;
}

namespace UPnPClient {

typedef std::shared_ptr<RenderingControl> RDCH;

/**
 * RenderingControl Service client class.
 *
 */
class RenderingControl : public Service {
public:

    /** Construct by copying data from device and service objects.
     *
     */
    RenderingControl(const UPnPDeviceDesc& device,
                     const UPnPServiceDesc& service);
    virtual ~RenderingControl();

    RenderingControl() {}

    /** Test service type from discovery message */
    static bool isRDCService(const std::string& st);

    /** @ret 0 for success, upnp error else */
    int setVolume(int volume, const std::string& channel = "Master");
    int getVolume(const std::string& channel = "Master");
    int setMute(bool mute, const std::string& channel = "Master");
    bool getMute(const std::string& channel = "Master");

protected:
    /* My service type string */
    static const std::string SType;

    /* Volume settings params */
    int m_volmin;
    int m_volmax;
    int m_volstep;

private:
    void evtCallback(const std::unordered_map<std::string, std::string>&);
    void registerCallback();
    /** Set volume parameters from service state variable table values */
    void setVolParams(int min, int max, int step);
    int devVolTo0100(int);
};

} // namespace UPnPClient

#endif /* _RENDERINGCONTROL_HXX_INCLUDED_ */
