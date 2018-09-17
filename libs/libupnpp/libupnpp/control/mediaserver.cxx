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
#include "libupnpp/config.h"

#include "libupnpp/control/mediaserver.hxx"

#include <unordered_map>
#include <ostream>                      // for endl
#include <string>                       // for string
#include <utility>                      // for pair
#include <vector>                       // for vector

#include "libupnpp/control/cdirectory.hxx"  // for ContentDirectory, CDSH
#include "libupnpp/control/description.hxx"  // for UPnPDeviceDesc, etc
#include "libupnpp/control/discovery.hxx"  // for UPnPDeviceDirectory, etc
#include "libupnpp/log.hxx"             // for LOGERR

using namespace std;
using namespace std::placeholders;
using namespace UPnPP;

namespace UPnPClient {

const string
MediaServer::DType("urn:schemas-upnp-org:device:MediaServer:1");

// We don't include a version in comparisons, as we are satisfied with
// version 1
bool MediaServer::isMSDevice(const string& st)
{
    const string::size_type sz(DType.size()-2);
    return !DType.compare(0, sz, st, 0, sz);
}

static bool MDAccum(std::unordered_map<string, UPnPDeviceDesc>* out,
                    const string& friendlyName,
                    const UPnPDeviceDesc& device,
                    const UPnPServiceDesc& service)
{
    //LOGDEB("MDAccum: friendlyname: " << friendlyName <<
    //    " dev friendlyName " << device.friendlyName << endl);
    if (ContentDirectory::isCDService(service.serviceType) &&
            (friendlyName.empty() ? true :
             !friendlyName.compare(device.friendlyName))) {
        //LOGDEB("MDAccum setting " << device.UDN << endl);
        (*out)[device.UDN] = device;
    }
    return true;
}

bool MediaServer::getDeviceDescs(vector<UPnPDeviceDesc>& devices,
                                 const string& friendlyName)
{
    std::unordered_map<string, UPnPDeviceDesc> mydevs;

    UPnPDeviceDirectory::Visitor visitor = bind(MDAccum, &mydevs, friendlyName,
                                           _1, _2);
    UPnPDeviceDirectory::getTheDir()->traverse(visitor);
    for (std::unordered_map<string, UPnPDeviceDesc>::iterator it = mydevs.begin();
            it != mydevs.end(); it++)
        devices.push_back(it->second);
    return !devices.empty();
}

MediaServer::MediaServer(const UPnPDeviceDesc& desc)
    : Device(desc)
{
    bool found = false;
    for (vector<UPnPServiceDesc>::const_iterator it = desc.services.begin();
            it != desc.services.end(); it++) {
        if (ContentDirectory::isCDService(it->serviceType)) {
            m_cds = CDSH(new ContentDirectory(desc, *it));
            found = true;
            break;
        }
    }
    if (!found) {
        LOGERR("MediaServer::MediaServer: ContentDirectory service not "
               "found in device" << endl);
    }
}

}
