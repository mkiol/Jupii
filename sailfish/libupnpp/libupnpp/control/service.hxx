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
#include "libupnpp/config.h"

#include <sys/types.h>
#include <upnp/upnp.h>                  // for UPNP_E_BAD_RESPONSE, etc

#include <functional>
#include <iostream>                     // for basic_ostream, operator<<, etc
#include <string>                       // for string, operator<<, etc

#include <vector>                       // for vector

#include "libupnpp/control/cdircontent.hxx"  // for UPnPDirObject
#include "libupnpp/log.hxx"             // for LOGERR
#include "libupnpp/soaphelp.hxx"        // for SoapIncoming, etc

namespace UPnPClient {
class UPnPDeviceDesc;
}
namespace UPnPClient {
class UPnPServiceDesc;
}

//using namespace UPnPP;

namespace UPnPClient {

class Service;

/** To be implemented by upper-level client code for event
 * reporting. Runs in an event thread. This could for example be
 * implemented by a Qt Object to generate events for the GUI.
 */
class VarEventReporter {
public:
    virtual ~VarEventReporter() {}
    // Using char * to avoid any issue with strings and concurrency
    virtual void changed(const char *nm, int val)  = 0;
    virtual void changed(const char *nm, const char *val) = 0;
    // Used for track metadata (parsed as content directory entry). Not always
    // needed.
    virtual void changed(const char * /*nm*/, UPnPDirObject /*meta*/) {}
    // Used by ohplaylist. Not always needed
    virtual void changed(const char * /*nm*/, std::vector<int> /*ids*/) {}
};

typedef
std::function<void (const std::unordered_map<std::string, std::string>&)>
evtCBFunc;

class Service {
public:
    /** Construct by copying data from device and service objects.
     */
    Service(const UPnPDeviceDesc& device, const UPnPServiceDesc& service);

    /** An empty one */
    Service();

    virtual ~Service();

    // This can be useful to restart the subscription and get all the
    // State variable values, in case we get the events before we are
    // ready (e.g. before the connections are set in a qt app
    // we'll do this next abi change virtual void reSubscribe();

    const std::string& getFriendlyName() const;
    const std::string& getDeviceId() const;
    const std::string& getServiceType() const;
    const std::string& getActionURL() const;
    const std::string& getModelName() const;
    const std::string& getManufacturer() const;

    virtual int runAction(const UPnPP::SoapOutgoing& args,
                          UPnPP::SoapIncoming& data);

    /** Run trivial action where there are neither input parameters
       nor return data (beyond the status) */
    int runTrivialAction(const std::string& actionName);

    /* Run action where there are no input parameters and a single
       named value is to be retrieved from the result */
    template <class T> int runSimpleGet(const std::string& actnm,
                                        const std::string& valnm,
                                        T *valuep);

    /* Run action with a single input parameter and no return data */
    template <class T> int runSimpleAction(const std::string& actnm,
                                           const std::string& valnm,
                                           T value);

    virtual VarEventReporter *getReporter();

    virtual void installReporter(VarEventReporter* reporter);


protected:

    /** Used by a derived class to register its callback method. This
     * creates an entry in the static map, using m_SID, which was
     * obtained by subscribe() during construction
     */
    void registerCallback(evtCBFunc c);
    void unregisterCallback();

private:
    // Can't copy these because this does not make sense for the
    // member function callback.
    Service(Service const&);
    Service& operator=(Service const&);

    class Internal;
    Internal *m;

    /* Only actually does something on the first call, to register our
     * (static) library callback */
    static bool initEvents();
    /* The static event callback given to libupnp */
    static int srvCB(Upnp_EventType et, void* vevp, void*);
    /* Tell the UPnP device (through libupnp) that we want to receive
       its events. This is called by registerCallback() and sets m_SID */
    virtual bool subscribe();
    virtual bool unSubscribe();
};

} // namespace UPnPClient

#endif /* _SERVICE_H_X_INCLUDED_ */
