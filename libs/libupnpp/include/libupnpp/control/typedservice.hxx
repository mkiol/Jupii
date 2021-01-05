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
#ifndef _TYPEDSERVICE_H_X_INCLUDED_
#define _TYPEDSERVICE_H_X_INCLUDED_

#include <string>
#include <vector>
#include <map>

#include "libupnpp/control/service.hxx"

namespace UPnPClient {

/** Access an UPnP service actions through a string based interface.
 *
 * This class allows flexible and easy access to a service functionality 
 * without the effort of deriving a specific class from UPnPClient::Service. 
 * It was mostly implemented for the Python SWIG interface, but it could 
 * probably have other usages.
 * The constructor yields a barely initialized object, still needing
 * initialization through a call to Service::initFromDescription(). 
 * The easiest way to build a usable object is to use the 
 * findTypedService() helper function.
 */
class UPNPP_API TypedService : public Service {
public:

    /** Build an empty object. Will be later initialized by 
     * initFromDescription(), typically called from findTypedService().
     * @param tp should be the official service type value, e.g.
     *    urn:schemas-upnp-org:service:AVTransport:1
     */
    TypedService(const std::string& tp);

    virtual ~TypedService();

    /** Check if the input matches our service type */
    virtual bool serviceTypeMatch(const std::string& tp);

    /** Run an action specified by name, with specified input return output.
     * @param name the action name (e.g. SetAVTransportURI)
     * @param args the input argument vector. These *must* be given in the 
     *   order given by the action definition inside the service description.
     * @param[out] retdata the output returned from the action. 
     *       map used instead of unordered_map for swig 2.0 compatibility.
     * @return a libupnp error code, 0 for success.
     */
    virtual int runAction(const std::string& name,
                          std::vector<std::string> args,
                          std::map<std::string, std::string>& retdata);

protected:
    /** Service-specific part of initialization. This downloads and parses 
     * the service description data. This is called from initFromDescription(),
     * typically in findTypedService() in our case. */
    virtual bool serviceInit(const UPnPDeviceDesc& device,
                             const UPnPServiceDesc& service);

private:
    // Suppress warning about the normal Service class runAction being
    // hidden.  Maybe it could be used, but we don't really need it
    // (it's called by our own runAction()).
    using Service::runAction;
    class UPNPP_LOCAL Internal;
    Internal *m{0};
    TypedService();
    void UPNPP_LOCAL evtCallback(
        const std::unordered_map<std::string, std::string>&);
    void UPNPP_LOCAL registerCallback();
};


/** Find specified service inside specified device, and build a
 *   TypedService object.
 * @param devname the device identifier can be specified as an UDN or
 *   a friendly name. Beware that friendly names are not necessarily
 *   unique. In case of duplicates the first (random) device found will
 *   be used. Comparisons between friendly names are case-insensitive (not 
 *   conform to the standard but convenient).
 * @param servicetype Depending on the value of fuzzy, this will either 
 *   be used to match the service type exactly, or as a partial match: for 
 *   example with fuzzy set to true, a servicetype of "avtransport" would
 *   match "urn:schemas-upnp-org:service:AVTransport:1", which is what
 *   you want in general, with a bit of care.
 * @param fuzzy determines if the service type match is exact or partial.
 * @return an allocated TypedService. Ownership is tranferred to the caller, 
 *   who will have to delete the object when done.
 */
TypedService UPNPP_API *findTypedService(
    const std::string& devname, const std::string& servicetype, bool fuzzy);

} // namespace UPnPClient

#endif // _TYPEDSERVICE_H_X_INCLUDED_
