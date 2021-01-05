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
#ifndef _UPNPDEV_HXX_INCLUDED_
#define _UPNPDEV_HXX_INCLUDED_

/**
 * UPnP Description phase: interpreting the device description which we
 * downloaded from the URL obtained by the discovery phase.
 */

#include <unordered_map>
#include <vector>
#include <string>
#include <sstream>

#include "libupnpp/upnppexports.hxx"

namespace UPnPClient {

/** Data holder for a UPnP service, parsed from the device XML description.
 * The discovery code does not download the service description
 * documents, and the only set values after discovery are those available from 
 * the device description (serviceId, SCPDURL, controlURL, eventSubURL).
 * You can call fetchAndParseDesc() to obtain a parsed version of the
 * service description document, with all the actions and state
 * variables. This is mostly useful if you need to retrieve some min/max values
 * for the state variable, in case there is no action defined to
 * retrieve them (e.g. min/max volume values for AVT RenderingControl). 
 * Also, if you wanted to define dynamic methods from the description data.
 */
class UPNPP_API UPnPServiceDesc {
public:
    /// Service Type e.g. urn:schemas-upnp-org:service:ConnectionManager:1
    std::string serviceType;
    /// Service Id inside device: e.g. urn:upnp-org:serviceId:ConnectionManager
    std::string serviceId; 
    /// Service description URL.
    std::string SCPDURL;
    /// Service control URL.
    std::string controlURL; 
    /// Service event URL.
    std::string eventSubURL;

    /// Reset all data
    void clear() {
        serviceType.clear();
        serviceId.clear();
        SCPDURL.clear();
        controlURL.clear();
        eventSubURL.clear();
    }

    /// Debug: return the basic parsed data as a string
    std::string dump() const {
        std::ostringstream os;
        os << "SERVICE {serviceType [" << serviceType <<
            "] serviceId [" << serviceId <<
            "] SCPDURL [" << SCPDURL <<
            "] controlURL [" << controlURL <<
            "] eventSubURL [" << eventSubURL <<
            "] }" << std::endl;
        return os.str();
    }

    /** Description of an action argument: name, direction, state
        variable it relates to (which will yield the type) */
    struct Argument {
        std::string name;
        bool todevice;
        std::string relatedVariable;
        void clear() {
            name.clear();
            todevice = true;
            relatedVariable.clear();
        }
    };

    /** UPnP service action descriptor, from the service description document*/
    struct Action {
        std::string name;
        std::vector<Argument> argList;
        void clear() {
            name.clear();
            argList.clear();
        }
    };

    /** Holder for all the attributes of an UPnP service state variable */
    struct StateVariable {
        std::string name;
        bool sendEvents;
        std::string dataType;
        bool hasValueRange;
        int minimum;
        int maximum;
        int step;
        void clear() {
            name.clear();
            sendEvents = false;
            dataType.clear();
            hasValueRange = false;
        }
    };

    /** Service description as parsed from the service XML document: actions 
     * and state variables */
    struct Parsed {
        std::unordered_map<std::string, Action> actionList;
        std::unordered_map<std::string, StateVariable> stateTable;
    };

    /** Fetch the service description document and parse it. 
     * @param urlbase The URL base is found in  the device description 
     * @param[out] parsed The resulting parsed Action and Variable lists.
     * @param[out] XMLText The raw downloaded XML text.
     */
    bool fetchAndParseDesc(const std::string& urlbase, Parsed& parsed,
                           std::string *XMLText = 0) const;
};

/**
 * Data holder for a UPnP device, parsed from the XML description obtained
 * during discovery. The object is built by the discovery code. 
 * User-level code gets access to the data by using the device directory 
 * traversal methods.
 */
class UPNPP_API UPnPDeviceDesc {
public:
    /** Build device from the XML description downloaded during discovery.
     * This is an internal library call, used from the discovery module.
     * The user code gets access to an initialized Device Description
     * object through the device directory traversal methods.
     * @param url where the description came from
     * @param description the xml device description
     */
    UPnPDeviceDesc(const std::string& url, const std::string& description);

    UPnPDeviceDesc() {}

    /// Parse success status.
    bool ok{false};
    /// Device Type: e.g. urn:schemas-upnp-org:device:MediaServer:1
    std::string deviceType;
    /// User-configurable name (usually), e.g. Lounge-streamer
    std::string friendlyName;
    /// Unique Device Number. This is the same as the deviceID in the
    /// discovery message. e.g. uuid:a7bdcd12-e6c1-4c7e-b588-3bbc959eda8d
    std::string UDN;
    /// Base for all relative URLs. e.g. http://192.168.4.4:49152/
    std::string URLBase;
    /// Manufacturer: e.g. D-Link, PacketVideo
    std::string manufacturer;
    /// Model name: e.g. MediaTomb, DNS-327L
    std::string modelName;

    /// Raw downloaded document.
    std::string XMLText;
    
    /// Services provided by this device.
    std::vector<UPnPServiceDesc> services;

    /// Embedded devices. We use UPnPDeviceDesc for convenience, but
    /// they can't recursively have embedded devices (and they just get
    /// a copy of the root URLBase).
    std::vector<UPnPDeviceDesc> embedded;

    void clear() {
        *this = UPnPDeviceDesc();
    }
    std::string dump() const {
        std::ostringstream os;
        os << "DEVICE " << " {deviceType [" << deviceType <<
            "] friendlyName [" << friendlyName <<
            "] UDN [" << UDN <<
            "] URLBase [" << URLBase << "] Services:" << std::endl;
        for (std::vector<UPnPServiceDesc>::const_iterator it = services.begin();
             it != services.end(); it++) {
            os << "    " << it->dump();
        }
        for (const auto& it: embedded) {
            os << it.dump();
        }
        os << "}" << std::endl;
        return os.str();
    }
};

} // namespace

#endif /* _UPNPDEV_HXX_INCLUDED_ */
