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
#ifndef _OHINFO_HXX_INCLUDED_
#define _OHINFO_HXX_INCLUDED_

#include <unordered_map>
#include <string>
#include <vector>

#include "service.hxx"


namespace UPnPClient {

class OHInfo;
class UPnPDeviceDesc;
class UPnPServiceDesc;

typedef std::shared_ptr<OHInfo> OHIFH;

/**
 * OHInfo Service client class.
 */
class UPNPP_API OHInfo : public Service {
public:

    OHInfo(const UPnPDeviceDesc& device, const UPnPServiceDesc& service)
        : Service(device, service) {
    }

    OHInfo() {}

    ~OHInfo() {}

    /** Test service type from discovery message */
    static bool isOHInfoService(const std::string& st);
    virtual bool serviceTypeMatch(const std::string& tp);

    int metatext(UPnPDirObject *dirent);

protected:
    /* My service type string */
    static const std::string SType;

private:
    void UPNPP_LOCAL evtCallback(
        const std::unordered_map<std::string, std::string>&);
    void UPNPP_LOCAL registerCallback();
};

} // namespace UPnPClient

#endif /* _OHINFO_HXX_INCLUDED_ */
