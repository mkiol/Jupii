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
#ifndef _OHTIME_HXX_INCLUDED_
#define _OHTIME_HXX_INCLUDED_

#include <unordered_map>
#include <memory>
#include <string>
#include <vector>

#include "libupnpp/control/service.hxx"

namespace UPnPClient {

class OHTime;
class UPnPDeviceDesc;
class UPnPServiceDesc;

typedef std::shared_ptr<OHTime> OHTMH;

/**
 * OHTime Service client class.
 *
 */
class UPNPP_API OHTime : public Service {
public:

    OHTime(const UPnPDeviceDesc& device, const UPnPServiceDesc& service)
        : Service(device, service) {
    }
    OHTime() {}
    virtual ~OHTime() {}

    /** Test service type from discovery message */
    static bool isOHTMService(const std::string& st);
    virtual bool serviceTypeMatch(const std::string& tp);

    struct Time {
        int trackCount;
        int duration;
        int seconds;
    };
    int time(Time&);

protected:
    /* My service type string */
    static const std::string SType;

private:
    void UPNPP_LOCAL evtCallback(
        const std::unordered_map<std::string, std::string>&);
    void UPNPP_LOCAL registerCallback();
};

} // namespace UPnPClient

#endif /* _OHTIME_HXX_INCLUDED_ */
