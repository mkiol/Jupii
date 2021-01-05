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

#include <string>

#include "service.hxx"


namespace UPnPClient {

class RenderingControl;
class UPnPDeviceDesc;
class UPnPServiceDesc;

typedef std::shared_ptr<RenderingControl> RDCH;

/**
 * RenderingControl Service client class.
 *
 */
class UPNPP_API RenderingControl : public Service {
public:

    /** Construct by copying data from device and service objects. */
    RenderingControl(const UPnPDeviceDesc& device,
                     const UPnPServiceDesc& service);

    RenderingControl() {}
    virtual ~RenderingControl() {}

    /** Test service type from discovery message */
    static bool isRDCService(const std::string& st);
    virtual bool serviceTypeMatch(const std::string& tp);

    /** Set volume to input value (0-100).
     * @return 0 for success, upnp error else 
     */
    int setVolume(int volume, const std::string& channel = "Master");
    /** @return current volume value (0-100) or negative for error. */
    int getVolume(const std::string& channel = "Master");
    int setMute(bool mute, const std::string& channel = "Master");
    bool getMute(const std::string& channel = "Master");

protected:
    virtual bool serviceInit(const UPnPDeviceDesc& device,
                             const UPnPServiceDesc& service);

    /* My service type string */
    static const std::string SType;

    /* Volume settings params */
    int m_volmin{0};
    int m_volmax{100};
    int m_volstep{1};

private:
    void UPNPP_LOCAL evtCallback(
        const std::unordered_map<std::string, std::string>&);
    void UPNPP_LOCAL registerCallback();
    /** Set volume parameters from service state variable table values */
    void UPNPP_LOCAL setVolParams(int min, int max, int step);
    int UPNPP_LOCAL devVolTo0100(int);
};

} // namespace UPnPClient

#endif /* _RENDERINGCONTROL_HXX_INCLUDED_ */
