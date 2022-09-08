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
#ifndef _AVLASTCHG_H_X_INCLUDED_
#define _AVLASTCHG_H_X_INCLUDED_

#include "libupnpp/config.h"

#include <unordered_map>
#include <string>

namespace UPnPClient {

/** Helper function for decoding UPnP/AV LastChange data.
 *    <Event xmlns="urn:schemas-upnp-org:metadata-1-0/AVT_RCS">
 *      <InstanceID val="0">
 *        <Mute val="0"/>
 *        <Volume val="24"/>
 *      </InstanceID>
 *    </Event>
 */
extern bool decodeAVLastChange(const std::string& xml,
                               std::unordered_map<std::string,
                               std::string>& props);


} // namespace UPnPClient

#endif /* _AVLASTCHG_H_X_INCLUDED_ */
