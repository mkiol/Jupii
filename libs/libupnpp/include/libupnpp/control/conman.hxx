/* Copyright (C) 2006-2019 J.F.Dockes
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
#ifndef _CONMAN_H_X_INCLUDED_
#define _CONMAN_H_X_INCLUDED_

#include "libupnpp/control/typedservice.hxx"
#include "libupnpp/upnpavutils.hxx"

namespace UPnPClient {

class ConnectionManager;
typedef std::shared_ptr<ConnectionManager> CNMH;

class UPNPP_API ConnectionManager : public TypedService {
public:
    ConnectionManager(const std::string& tp)
        : TypedService(tp) {
    }

    int getProtocolInfo(std::vector<UPnPP::ProtocolinfoEntry>& sourceEntries,
                        std::vector<UPnPP::ProtocolinfoEntry>& sinkEntries);
    
    static bool isConManService(const std::string& st);
    virtual bool serviceTypeMatch(const std::string& tp);
};
    
} // namespace

#endif /* _CONMAN_H_X_INCLUDED_ */
