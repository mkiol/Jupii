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
#ifndef _OHRECEIVER_HXX_INCLUDED_
#define _OHRECEIVER_HXX_INCLUDED_

#include <unordered_map>
#include <string>
#include <vector>

#include "libupnpp/control/service.hxx"
#include "libupnpp/control/ohplaylist.hxx"


namespace UPnPClient {

class UPnPDeviceDesc;
class UPnPServiceDesc;
class OHReceiver;

typedef std::shared_ptr<OHReceiver> OHRCH;

/**
 * OHReceiver client class.
 */
class UPNPP_API OHReceiver : public Service {
public:

    OHReceiver(const UPnPDeviceDesc& device, const UPnPServiceDesc& service)
        : Service(device, service) {
    }
    OHReceiver() {}
    virtual ~OHReceiver() {}

    /** Test service type from discovery message */
    static bool isOHRcService(const std::string& st);
    virtual bool serviceTypeMatch(const std::string& tp);

    int play();
    int stop();
    int setSender(const std::string& uri, const std::string& meta);
    int sender(std::string& uri, std::string& meta);
    int protocolInfo(std::string *proto);
    int transportState(OHPlaylist::TPState *tps);

protected:
    /* My service type string */
    static const std::string SType;

private:
    void UPNPP_LOCAL evtCallback(
        const std::unordered_map<std::string, std::string>&);
    void UPNPP_LOCAL registerCallback();
};

} // namespace UPnPClient

#endif /* _OHRECEIVER_HXX_INCLUDED_ */
