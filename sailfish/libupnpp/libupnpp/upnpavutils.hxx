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
#ifndef _UPNPAVUTILS_HXX_INCLUDED_
#define _UPNPAVUTILS_HXX_INCLUDED_

#include <string>
#include <vector>
#include <unordered_map>

namespace UPnPP {

/** Format milliseconds into H+:MM:SS */
extern std::string upnpduration(int ms);

/** H+:MM:SS to seconds */
extern int upnpdurationtos(const std::string& dur);

/** Decode OH playlist id array */
extern bool ohplIdArrayToVec(const std::string& data, std::vector<int> *ids);

/** Decoded protocolinfo entry data */
struct ProtocolinfoEntry {
    std::string protocol;
    std::string network;
    std::string contentFormat;
    std::unordered_map<std::string, std::string> content_params;
    std::string additional;
};
    
/** Decode one protocolinfo entry */
extern bool parseProtoInfEntry(const std::string& protoinfo,
                               ProtocolinfoEntry& e);

/** Decode comma-separated bunch of entries */
bool parseProtocolInfo(const std::string& pinfo,
                       std::vector<ProtocolinfoEntry>& entries);
    
}

#endif /* _UPNPAVUTILS_HXX_INCLUDED_ */
