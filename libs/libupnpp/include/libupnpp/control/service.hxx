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

#include <sys/types.h>

#include <functional>
#include <iostream>
#include <string>

#include "libupnpp/upnppexports.hxx"
#include "libupnpp/log.hxx"
#include "libupnpp/soaphelp.hxx"
#include "libupnpp/upnperrcodes.hxx"
#include "libupnpp/control/cdircontent.hxx"

namespace UPnPClient {

class UPnPDeviceDesc;
class UPnPServiceDesc;
class Service;

/** To be implemented by upper-level client code for event reporting. 
 *
 * Runs in an event thread. This could for example be
 * implemented by a Qt Object to generate events for the GUI.
 *
 * This is used by all "precooked" UPnP/AV service classes in the
   library. The derived Service class does a bit of parsing for common
   cases.
 * The different methods cover all current types of audio UPnP
 * state variable data I am aware of. Of course, other types of data can 
 * be reported as a character string, leaving the parsing to the client code.
 *
 * In the general case, you could also derive from Service, implement
 * and install an evtCBFunc callback and not use VarEventReporter at
 * all.
 */
class UPNPP_API VarEventReporter {
public:
    virtual ~VarEventReporter() {}
    /** Report change to named integer state variable */
    virtual void changed(const char *nm, int val)  = 0;
    /** Report change to named character string state variable */
    virtual void changed(const char *nm, const char *val) = 0;
    /** Report change to track metadata (parsed as as Content
     * Directory entry). Not always needed */
    virtual void changed(const char * /*nm*/, UPnPDirObject /*meta*/) {}
    /** Special for  ohplaylist. Not always needed */
    virtual void changed(const char * /*nm*/, std::vector<int> /*ids*/) {}
    /** Subscription autorenew failed. You may want to schedule a
       resubscribe() later. */
    virtual void autorenew_failed() {}
};

/** Type of the event callbacks. 
 * If registered by a call to Service::registerCallBack(cbfunc), this will be
 * called with a map of state variable names and values when 
 * an event arrives. The call is performed in a separate thread. 
 * A call with an empty map means that a subscription autorenew failed.
 */
typedef
std::function<void (const std::unordered_map<std::string, std::string>&)>
evtCBFunc;

class UPNPP_API Service {
public:
    /** Construct by copying data from device and service objects. */
    Service(const UPnPDeviceDesc& device, const UPnPServiceDesc& service);
    
    /** Empty object. 
     * May be initialized later by calling initFromDescription().
     */
    Service();

    virtual ~Service();

    /** Initialize empty object from device description. 
     * This allows separating the object construction and initialization.
     * The method can fail if the appropriate service is not found. 
     * It calls serviceInit() to perform any initialization specific to the 
     * service type. This relies on serviceTypeMatch() implemented in the 
     * derived class to find the right service.
     */
    bool initFromDescription(const UPnPDeviceDesc& description);
    
    /** Restart the subscription to get all the State variable values,
     * in case we get the events before we are ready (e.g. before the
     * connections are set in a qt app). Also: when reconnecting after
     * a device restarts. */
    virtual bool reSubscribe();

    /** Accessors for the values extracted from the device description during 
     *  initialization */
    const std::string& getFriendlyName() const;
    const std::string& getDeviceId() const;
    const std::string& getServiceType() const;
    const std::string& getActionURL() const;
    const std::string& getModelName() const;
    const std::string& getManufacturer() const;

    /**
     * Call Soap action and return resulting data.
     * @param args Action name and input parameters
     * @param data return data.
     * @return 0 if the call succeeded, some non-zero UPNP_E_... value else
     */
    virtual int runAction(const UPnPP::SoapOutgoing& args,
                          UPnPP::SoapIncoming& data);

    /** Run trivial action where there are neither input parameters
        nor return data (beyond the status) */
    int runTrivialAction(const std::string& actionName);

    /** Run action where there are no input parameters and a single
     * named value is to be retrieved from the result */
    template <class T> int runSimpleGet(const std::string& actnm,
                                        const std::string& valnm,
                                        T *valuep);

    /** Run action with a single input parameter and no return data */
    template <class T> int runSimpleAction(const std::string& actnm,
                                           const std::string& valnm,
                                           T value);

    /** Get pointer to installed event reporter
     *
     * This is used by a derived class event handling method and
     * should be in the protected section actually, it has no external
     * use which I can think of
     */
    virtual VarEventReporter *getReporter();

    /** Install or uninstall event data reporter object. 
     *  @param  reporter the callbacks to be installed, or nullptr 
     *  to disable reporting (and cancel the upnp subscription). 
     */
    virtual void installReporter(VarEventReporter* reporter);

    /** Perform a comparison to the service type string for this specific 
     *  service.
     *  This allows embedding knowledge of the service type string inside the 
     *  derived class. It is used, e.g., by initFromDescription() to look up 
     *  an appropriate entry from the device description service list. 
     *  Can also be used by external code wishing to do the same.
     *  @param tp Service type string to be compared with the one for the 
     *       derived class.
     */
    virtual bool serviceTypeMatch(const std::string& tp) = 0;
    
protected:

    /** Service-specific part of initialization. 
     * This can be called from the constructor or from initFromDescription(). 
     * Most services don't need specific initialization, so we provide a 
     * default implementation.
     */
    virtual bool serviceInit(const UPnPDeviceDesc&,
                             const UPnPServiceDesc&) {
        return true;
    }

    /** Used by a derived class to register its callback method. This
     * creates an entry in the static map, using m_SID, which was
     * obtained by subscribe() during construction
     */
    bool registerCallback(evtCBFunc c);

    /** To be overridden in classes which actually support events. Will be
     * called by installReporter(). The call sequence is as follows:
     * some_client_code()
     *   Service::installReporter()
     *     derived::registerCallback()
     *       Service::registerCallback(derivedcbfunc)
     */
    virtual void registerCallback() {}

    /** Cancel subscription to the service events, forget installed callback */
    void unregisterCallback();

private:
    // Can't copy these because this does not make sense for the
    // member function callback.
    Service(Service const&);
    Service& operator=(Service const&);

    class UPNPP_LOCAL Internal;
    Internal *m{nullptr};
};

} // namespace UPnPClient

#endif /* _SERVICE_H_X_INCLUDED_ */
