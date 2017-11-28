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
#include "libupnpp/control/ohtime.hxx"

#include <stdlib.h>                     // for atoi
#include <string.h>                     // for strcmp
#include <upnp/upnp.h>                  // for UPNP_E_BAD_RESPONSE, etc

#include <functional>                   // for _Bind, bind, _1
#include <ostream>                      // for endl, basic_ostream, etc
#include <string>                       // for string, basic_string, etc

#include "libupnpp/control/service.hxx"  // for VarEventReporter, Service
#include "libupnpp/log.hxx"             // for LOGERR, LOGDEB1, LOGINF
#include "libupnpp/soaphelp.hxx"        // for SoapIncoming, etc
#include "libupnpp/upnpp_p.hxx"         // for stringToBool

using namespace std;
using namespace std::placeholders;
using namespace UPnPP;

namespace UPnPClient {

const string OHTime::SType("urn:av-openhome-org:service:Time:1");

// Check serviceType string (while walking the descriptions. We don't
// include a version in comparisons, as we are satisfied with version1
bool OHTime::isOHTMService(const string& st)
{
    const string::size_type sz(SType.size()-2);
    return !SType.compare(0, sz, st, 0, sz);
}

void OHTime::evtCallback(
    const std::unordered_map<std::string, std::string>& props)
{
    LOGDEB1("OHTime::evtCallback: getReporter(): " << getReporter() << endl);
    for (std::unordered_map<std::string, std::string>::const_iterator it =
                props.begin(); it != props.end(); it++) {
        if (!getReporter()) {
            LOGDEB1("OHTime::evtCallback: " << it->first << " -> "
                    << it->second << endl);
            continue;
        }

        if (!it->first.compare("TrackCount") ||
                !it->first.compare("Duration") ||
                !it->first.compare("Seconds")) {

            getReporter()->changed(it->first.c_str(), atoi(it->second.c_str()));

        } else {
            LOGERR("OHTime event: unknown variable: name [" <<
                   it->first << "] value [" << it->second << endl);
            getReporter()->changed(it->first.c_str(), it->second.c_str());
        }
    }
}

void OHTime::registerCallback()
{
    Service::registerCallback(bind(&OHTime::evtCallback, this, _1));
}

int OHTime::time(Time& out)
{
    SoapOutgoing args(getServiceType(), "Time");
    SoapIncoming data;
    int ret = runAction(args, data);
    if (ret != UPNP_E_SUCCESS) {
        return ret;
    }
    if (!data.get("TrackCount", &out.trackCount)) {
        LOGERR("OHPlaylist::insert: missing 'TrackCount' in response" << endl);
        return UPNP_E_BAD_RESPONSE;
    }
    if (!data.get("Duration", &out.duration)) {
        LOGERR("OHPlaylist::insert: missing 'Duration' in response" << endl);
        return UPNP_E_BAD_RESPONSE;
    }
    if (!data.get("Seconds", &out.seconds)) {
        LOGERR("OHPlaylist::insert: missing 'Seconds' in response" << endl);
        return UPNP_E_BAD_RESPONSE;
    }
    return UPNP_E_SUCCESS;
}

} // End namespace UPnPClient
