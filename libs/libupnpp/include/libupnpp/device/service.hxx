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
#ifndef _SERVICE_H_X_INCLUDED_
#define _SERVICE_H_X_INCLUDED_

#include <string>
#include <vector>

#include "libupnpp/upnppexports.hxx"

namespace UPnPProvider {

class UpnpDevice;

/**
 * Upnp service base class. 
 *
 * The derived class implements the action methods, registers the mappings
 * with the device object, and implements the event data generation.
 */
class UPNPP_API UpnpService {
public:
    /**
     * The main role of the derived constructor is to register the
     * service action callbacks by calling UpnpDevice::addActionMapping(). 
     * The generic constructor registers the object with the device.
     *
     * @param stp Service type.
     * @param stp Service id.
     * @param xmlfn XML service description designator. Used within the
     *    Device::addService() method to retrieve the data from the 
     *    implementation librarian.
     * @param dev The device this service will be attached to, by calling its
     *    addService() method.
     * @param noevents If set, the service will function normally except 
     *    that no calls will be made to libupnp to broadcast events.
     *    This allows a service object to retain its possible internal 
     *    functions without being externally visible (in conjunction with a 
     *    description doc edit).
     */
    UpnpService(const std::string& stp,const std::string& sid,
                const std::string& xmlfn, UpnpDevice *dev, bool noevents=false);

    virtual ~UpnpService();

    /**
     * Retrieve event data.
     *
     * To be implemented by the derived class if it does generate event data.
     * Also called by the library 
     * Return name/value pairs for changed variables in the data arrays.
     *
     * @param all If true, treat all state variable as changed (return
     *   full state). This is set when calling after a control point subscribes,
     *   to retrieve all eventable data.
     * @param names Names of returned state variable
     * @param values Values of the returned state variables, parallel to names.
     */
    virtual bool getEventData(bool all, std::vector<std::string>& names,
                              std::vector<std::string>& values);

    UpnpDevice *getDevice();
    virtual const std::string& getServiceType() const;
    virtual const std::string& getServiceId() const;
    virtual const std::string& getXMLFn() const;

    /** Get value of the noevents property */
    bool noevents() const;

    /** 
     * Error number to string translation. UPnP error code values are
     * duplicated and mean different things for different services, so
     * this handles the common codes and calls serviceErrString which
     * should be overriden by the subclasses.
     */
    virtual const std::string errString(int error) const;

    virtual const std::string serviceErrString(int) const {
        return "";
    }

    // Common (service-type-independant) error codes
    enum UPnPError {
        UPNP_INVALID_ACTION = 401,
        UPNP_INVALID_ARGS = 402,
        UPNP_INVALID_VAR = 404,
        // This one mine...
        UPNP_ACTION_CONFLICT = 409,
        UPNP_ACTION_FAILED = 501,

        /* 600-699 common action errors */
        UPNP_ARG_VALUE_INVALID = 600,
        UPNP_ARG_VALUE_OUT_OF_RANGE = 601,
        UPNP_OPTIONAL_ACTION_NOT_IMPLEMENTED = 602,
        UPNP_OUT_OF_MEMORY = 603,
        UPNP_HUMAN_INTERVENTION_REQUIRED = 604,
        UPNP_STRING_ARGUMENT_TOO_LONG = 605,
        UPNP_ACTION_NOT_AUTHORIZED = 606,
        UPNP_SIGNATURE_FAILING = 607,
        UPNP_SIGNATURE_MISSING = 608,
        UPNP_NOT_ENCRYPTED = 609,
        UPNP_INVALID_SEQUENCE = 610,
        UPNP_INVALID_CONTROL_URLS = 611,
        UPNP_NO_SUCH_SESSION = 612,
    };

    
private:
    class UPNPP_LOCAL Internal;
    Internal *m;
};

} // End namespace UPnPProvider


#endif /* _SERVICE_H_X_INCLUDED_ */
