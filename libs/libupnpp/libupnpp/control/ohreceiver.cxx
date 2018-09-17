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
#include "libupnpp/control/ohreceiver.hxx"

#include <stdlib.h>                     // for atoi
#include <string.h>                     // for strcmp
#include <upnp/upnp.h>                  // for UPNP_E_BAD_RESPONSE, etc

#include <functional>                   // for _Bind, bind, _1
#include <ostream>                      // for endl, basic_ostream, etc
#include <string>                       // for string, basic_string, etc
#include <utility>                      // for pair
#include <vector>                       // for vector

#include "libupnpp/upnpavutils.hxx"
#include "libupnpp/control/service.hxx"  // for VarEventReporter, Service
#include "libupnpp/log.hxx"             // for LOGERR, LOGDEB1, LOGINF
#include "libupnpp/soaphelp.hxx"        // for SoapIncoming, etc
#include "libupnpp/upnpp_p.hxx"         // for stringToBool
#include "libupnpp/control/ohreceiver.hxx"

using namespace std;
using namespace std::placeholders;
using namespace UPnPP;

namespace UPnPClient {

const string OHReceiver::SType("urn:av-openhome-org:service:Receiver:1");

// Check serviceType string (while walking the descriptions. We don't
// include a version in comparisons, as we are satisfied with version1
bool OHReceiver::isOHRcService(const string& st)
{
    const string::size_type sz(SType.size()-2);
    return !SType.compare(0, sz, st, 0, sz);
}

void OHReceiver::evtCallback(
    const std::unordered_map<std::string, std::string>& props)
{
    LOGDEB1("OHReceiver::evtCallback:getReporter(): " << getReporter() << endl);
    for (std::unordered_map<std::string, std::string>::const_iterator it =
                props.begin(); it != props.end(); it++) {
        if (!getReporter()) {
            LOGDEB1("OHReceiver::evtCallback: " << it->first << " -> "
                    << it->second << endl);
            continue;
        }

        if (!it->first.compare("TransportState")) {
            OHPlaylist::TPState tp;
            OHPlaylist::stringToTpState(it->second, &tp);
            getReporter()->changed(it->first.c_str(), int(tp));
        } else if (!it->first.compare("Metadata")) {
            getReporter()->changed(it->first.c_str(),
                                   it->second.c_str());
        } else if (!it->first.compare("Uri")) {
            getReporter()->changed(it->first.c_str(),
                                   it->second.c_str());
        } else if (!it->first.compare("ProtocolInfo")) {
            getReporter()->changed(it->first.c_str(),
                                   it->second.c_str());
        } else {
            LOGERR("OHReceiver event: unknown variable: name [" <<
                   it->first << "] value [" << it->second << endl);
            getReporter()->changed(it->first.c_str(), it->second.c_str());
        }
    }
}

void OHReceiver::registerCallback()
{
    Service::registerCallback(bind(&OHReceiver::evtCallback, this, _1));
}

int OHReceiver::play()
{
    return runTrivialAction("Play");
}
int OHReceiver::stop()
{
    return runTrivialAction("Stop");
}

int OHReceiver::setSender(const string& uri, const string& didl)
{
    SoapOutgoing args(getServiceType(), "SetSender");
    args("Uri", uri)
    ("Metadata", didl);
    SoapIncoming data;
    return runAction(args, data);
}

int OHReceiver::sender(string& uri, string& meta)
{
    SoapOutgoing args(getServiceType(), "Sender");
    SoapIncoming data;
    int ret = runAction(args, data);
    if (ret != UPNP_E_SUCCESS) {
        return ret;
    }
    if (!data.get("Uri", &uri)) {
        LOGERR("OHReceiver::Sender: missing Uri in response" << endl);
        return UPNP_E_BAD_RESPONSE;
    }
    if (!data.get("Metadata", &meta)) {
        LOGERR("OHReceiver::Sender: missing Metadata in response" << endl);
        return UPNP_E_BAD_RESPONSE;
    }
    return 0;
}

int OHReceiver::transportState(OHPlaylist::TPState *tpp)
{
    string value;
    int ret;

    if ((ret = runSimpleGet("TransportState", "Value", &value))) {
        return ret;
    }
    
    return OHPlaylist::stringToTpState(value, tpp);
}

int OHReceiver::protocolInfo(std::string *proto)
{
    SoapOutgoing args(getServiceType(), "ProtocolInfo");
    SoapIncoming data;
    int ret = runAction(args, data);
    if (ret != UPNP_E_SUCCESS) {
        return ret;
    }
    if (!data.get("Value", proto)) {
        LOGERR("OHReceiver::protocolInfo: missing Value in response" << endl);
        return UPNP_E_BAD_RESPONSE;
    }
    return 0;
}

} // End namespace UPnPClient
