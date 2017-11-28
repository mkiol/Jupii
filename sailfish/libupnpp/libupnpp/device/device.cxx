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

#include "device.hxx"

#include <errno.h>
#include <time.h>
#include <string.h>

#include <iostream>
#include <sstream>
#include <utility>
#include <algorithm>
#include <mutex>
#include <condition_variable>

#include "libupnpp/log.hxx"
#include "libupnpp/ixmlwrap.hxx"
#include "libupnpp/upnpplib.hxx"
#include "libupnpp/upnpputils.hxx"
#include "libupnpp/upnpp_p.hxx"
#include "vdir.hxx"

using namespace std;
using namespace UPnPP;

namespace UPnPProvider {

class UpnpDevice::Internal {
public:
    /* Generate event: called by the device event loop, which polls
     * the services. */
    void notifyEvent(const std::string& serviceId,
                     const std::vector<std::string>& names,
                     const std::vector<std::string>& values);
    bool start();

    std::unordered_map<std::string, UpnpService*>::const_iterator
    findService(const std::string& serviceid);

    /* Per-device callback */
    int callBack(Upnp_EventType et, void* evp);

    UPnPP::LibUPnP *lib;
    std::string deviceId;
    std::string description;

    // We keep the services in a map for easy access from id and in a
    // vector for ordered walking while fetching status. Order is
    // determine by addService() call sequence.
    std::unordered_map<std::string, UpnpService*> servicemap;
    std::vector<std::string> serviceids;
    std::unordered_map<std::string, soapfun> calls;
    bool needExit;
    /* My device handle */
    UpnpDevice_Handle dvh;
    /* Lock for device operations. Held during a service callback
       Must not be held when using dvh to call into libupnp */
    std::mutex devlock;
    std::condition_variable evloopcond;
    std::mutex evlooplock;
};

class UpnpDevice::InternalStatic {
public:
    /* Static callback for libupnp. This looks up the appropriate
     * device using the device ID (UDN), the calls its callback
     * method */
    static int sCallBack(Upnp_EventType et, void* evp, void*);

    /** Static array of devices for dispatching */
    static std::unordered_map<std::string, UpnpDevice *> devices;
    static std::mutex devices_lock;
};

std::unordered_map<std::string, UpnpDevice *>
UpnpDevice::InternalStatic::devices;
std::mutex UpnpDevice::InternalStatic::devices_lock;
UpnpDevice::InternalStatic *UpnpDevice::o;

static bool vectorstoargslists(const vector<string>& names,
                               const vector<string>& values,
                               vector<string>& qvalues,
                               vector<const char *>& cnames,
                               vector<const char *>& cvalues)
{
    if (names.size() != values.size()) {
        LOGERR("vectorstoargslists: bad sizes" << endl);
        return false;
    }

    cnames.reserve(names.size());
    qvalues.clear();
    qvalues.reserve(values.size());
    cvalues.reserve(values.size());
    for (unsigned int i = 0; i < values.size(); i++) {
        cnames.push_back(names[i].c_str());
        qvalues.push_back(SoapHelp::xmlQuote(values[i]));
        cvalues.push_back(qvalues[i].c_str());
        //LOGDEB("Edata: " << cnames[i] << " -> " << cvalues[i] << endl);
    }
    return true;
}

static const int expiretime = 3600;

UpnpDevice::UpnpDevice(const string& deviceId,
                       const std::unordered_map<string, VDirContent>& files)
{
    if (o == 0 && (o = new InternalStatic()) == 0) {
        LOGERR("UpnpDevice::UpnpDevice: out of memory" << endl);
        return;
    }
    if ((m = new Internal()) == 0) {
        LOGERR("UpnpDevice::UpnpDevice: out of memory" << endl);
        return;
    }
    m->deviceId = deviceId;
    m->needExit = false;
    //LOGDEB("UpnpDevice::UpnpDevice(" << m->deviceId << ")" << endl);

    m->lib = LibUPnP::getLibUPnP(true);
    if (!m->lib) {
        LOGFAT(" Can't get LibUPnP" << endl);
        return;
    }
    if (!m->lib->ok()) {
        LOGFAT("Lib init failed: " <<
               m->lib->errAsString("main", m->lib->getInitError()) << endl);
        m->lib = 0;
        return;
    }

    {
        std::unique_lock<std::mutex> lock(o->devices_lock);
        if (o->devices.empty()) {
            // First call: init callbacks
            m->lib->registerHandler(UPNP_CONTROL_ACTION_REQUEST,
                                    o->sCallBack, this);
            m->lib->registerHandler(UPNP_CONTROL_GET_VAR_REQUEST,
                                    o->sCallBack, this);
            m->lib->registerHandler(UPNP_EVENT_SUBSCRIPTION_REQUEST,
                                    o->sCallBack, this);
        }
        o->devices[m->deviceId] = this;
    }

    if (!files.empty()) {
        // files will be empty for embedded devices, and hence
        // description too (we test description emptyness to
        // discriminate root/embedded in other places).
        VirtualDir* theVD = VirtualDir::getVirtualDir();
        if (theVD == 0) {
            LOGFAT("UpnpDevice::UpnpDevice: can't get VirtualDir" << endl);
            return;
        }

        for (const auto& it : files) {
            if (!path_getsimple(it.first).compare("description.xml")) {
                m->description = it.second.content;
                break;
            }
        }
        if (m->description.empty()) {
            LOGFAT("UpnpDevice::UpnpDevice: no description.xml in xmlfiles"
                   << endl);
            return;
        }

        for (const auto& it : files) {
            string dir = path_getfather(it.first);
            string fn = path_getsimple(it.first);
            // description.xml will be served by libupnp from / after
            // inserting the URLBase element (which it knows how to
            // compute), so we make sure not to serve our version from
            // the virtual dir (if it is in /, it would override
            // libupnp's).
            if (fn.compare("description.xml")) {
                theVD->addFile(dir, fn, it.second.content, it.second.mimetype);
            }
        }
    }
}

UpnpDevice::~UpnpDevice()
{
    if (!m->description.empty()) {
        UpnpUnRegisterRootDevice(m->dvh);
    }

    std::unique_lock<std::mutex> lock(o->devices_lock);
    auto it = o->devices.find(m->deviceId);
    if (it != o->devices.end())
        o->devices.erase(it);
}

bool UpnpDevice::ipv4(string *host, unsigned short *port) const
{
    char *hst = UpnpGetServerIpAddress();
    if (hst == 0) {
	return false;
    }
    if (port) {
	*port = UpnpGetServerPort();
    }
    if (host) {
	*host = string(hst);
    }
    return true;
}

bool UpnpDevice::Internal::start()
{
    // Start up the web server for sending out description files. This also
    // calls registerRootDevice()
    int ret;
    if (!description.empty()) {
        // Description is empty for embedded devices
        if ((ret = lib->setupWebServer(description, &dvh)) != 0) {
            LOGFAT("UpnpDevice: libupnp can't start service. Err " <<
                   ret << endl);
            return false;
        }
    }

    if ((ret = UpnpSendAdvertisement(dvh, expiretime)) != 0) {
        LOGERR(lib->errAsString("UpnpDevice: UpnpSendAdvertisement", ret)
               << endl);
        return false;
    }
    return true;
}

    
// Main libupnp callback: use the device id and call the right device
int UpnpDevice::InternalStatic::sCallBack(Upnp_EventType et, void* evp,
        void*)
{
    //LOGDEB("UpnpDevice::sCallBack" << endl);

    string deviceid;
    switch (et) {
    case UPNP_CONTROL_ACTION_REQUEST:
        deviceid = ((struct Upnp_Action_Request *)evp)->DevUDN;
        break;

    case UPNP_CONTROL_GET_VAR_REQUEST:
        deviceid = ((struct Upnp_State_Var_Request *)evp)->DevUDN;
        break;

    case UPNP_EVENT_SUBSCRIPTION_REQUEST:
        deviceid = ((struct  Upnp_Subscription_Request*)evp)->UDN;
        break;

    default:
        LOGERR("UpnpDevice::sCallBack: unknown event " << et << endl);
        return UPNP_E_INVALID_PARAM;
    }
    // LOGDEB("UpnpDevice::sCallBack: deviceid[" << deviceid << "]" << endl);

    std::unordered_map<std::string, UpnpDevice *>::iterator it;
    {
        std::unique_lock<std::mutex> lock(o->devices_lock);

        it = o->devices.find(deviceid);

        if (it == o->devices.end()) {
            LOGERR("UpnpDevice::sCallBack: Device not found: [" <<
                   deviceid << "]" << endl);
            return UPNP_E_INVALID_PARAM;
        }
    }

    // LOGDEB("UpnpDevice::sCallBack: device found: [" << it->second
    // << "]" << endl);
    return (it->second)->m->callBack(et, evp);
}

bool UpnpDevice::ok()
{
    return o && m && m->lib != 0;
}

std::unordered_map<string, UpnpService*>::const_iterator
UpnpDevice::Internal::findService(const string& serviceid)
{
    std::unique_lock<std::mutex> lock(devlock);
    auto servit = servicemap.find(serviceid);
    if (servit == servicemap.end()) {
        LOGERR("UpnpDevice: Bad serviceID: [" << serviceid << "]\n");
    }
    return servit;
}

int UpnpDevice::Internal::callBack(Upnp_EventType et, void* evp)
{
    switch (et) {
    case UPNP_CONTROL_ACTION_REQUEST:
    {
        struct Upnp_Action_Request *act = (struct Upnp_Action_Request *)evp;

        LOGDEB("UPNP_CONTROL_ACTION_REQUEST: " << act->ActionName <<
               ". Params: " << ixmlwPrintDoc(act->ActionRequest) << endl);

        std::unordered_map<string, UpnpService*>::const_iterator servit =
            findService(act->ServiceID);
        if (servit == servicemap.end()) {
            return UPNP_E_INVALID_PARAM;
        }

        SoapOutgoing dt(servit->second->getServiceType(), act->ActionName);
        {
            std::unique_lock<std::mutex> lock(devlock);

            std::unordered_map<std::string, soapfun>::iterator callit =
                calls.find(string(act->ActionName) + string(act->ServiceID));
            if (callit == calls.end()) {
                LOGINF("UpnpDevice: No such action: " <<
                       act->ActionName << endl);
                return UPNP_E_INVALID_PARAM;
            }

            SoapIncoming sc;
            if (!sc.decode(act->ActionName, act->ActionRequest)) {
                LOGERR("Error decoding Action call arguments" << endl);
                return UPNP_E_INVALID_PARAM;
            }

            // Call the action routine
            int ret = callit->second(sc, dt);
            if (ret != UPNP_E_SUCCESS) {
                if (ret > 0) {
                    act->ErrCode = ret;
                    strncpy(act->ErrStr, servit->second->errString(ret).c_str(),
                            LINE_SIZE-1);
                    act->ErrStr[LINE_SIZE-1] = 0;
                }
                LOGERR("UpnpDevice: Action failed: " << sc.getName() <<
                       " code " << ret << endl);
                return ret;
            }
        }

        // Encode result data
        act->ActionResult = dt.buildSoapBody();

        //LOGDEB("Response data: " << ixmlwPrintDoc(act->ActionResult) << endl);

        return UPNP_E_SUCCESS;
    }
    break;

    case UPNP_CONTROL_GET_VAR_REQUEST:
        // Note that the "Control: query for variable" action is
        // deprecated (upnp arch v1), and we should never get these.
    {
        struct Upnp_State_Var_Request *act =
            (struct Upnp_State_Var_Request *)evp;
        LOGDEB("UPNP_CONTROL_GET_VAR__REQUEST?: " << act->StateVarName << endl);
    }
    break;

    case UPNP_EVENT_SUBSCRIPTION_REQUEST:
    {
        struct Upnp_Subscription_Request *act =
            (struct  Upnp_Subscription_Request*)evp;
        LOGDEB("UPNP_EVENT_SUBSCRIPTION_REQUEST: " << act->ServiceId << endl);

        auto servit = findService(act->ServiceId);
        if (servit == servicemap.end()) {
            return UPNP_E_INVALID_PARAM;
        }

        vector<string> names, values, qvalues;
        {
            std::unique_lock<std::mutex> lock(devlock);
            if (!servit->second->getEventData(true, names, values)) {
                break;
            }
        }

        vector<const char *> cnames, cvalues;
        vectorstoargslists(names, values, qvalues, cnames, cvalues);
        int ret =
            UpnpAcceptSubscription(dvh, act->UDN, act->ServiceId,
                                   &cnames[0], &cvalues[0],
                                   int(cnames.size()), act->Sid);
        if (ret != UPNP_E_SUCCESS) {
            LOGERR(lib->errAsString(
                       "UpnpDevice::callBack: UpnpAcceptSubscription", ret) <<
                   endl);
        }

        return ret;
    }
    break;

    default:
        LOGINF("UpnpDevice::callBack: unknown libupnp event type: " <<
               LibUPnP::evTypeAsString(et).c_str() << endl);
        return UPNP_E_INVALID_PARAM;
    }
    return UPNP_E_INVALID_PARAM;
}

void UpnpDevice::addService(UpnpService *serv, const std::string& serviceId)
{
    LOGDEB("UpnpDevice::addService: [" << serviceId << "]\n");
    std::unique_lock<std::mutex> lock(m->devlock);

    m->servicemap[serviceId] = serv;
    vector<string>::iterator it =
        std::find(m->serviceids.begin(), m->serviceids.end(), serviceId);
    if (it != m->serviceids.end())
        m->serviceids.erase(it);
    m->serviceids.push_back(serviceId);
}

void UpnpDevice::forgetService(const std::string& serviceId)
{
    LOGDEB("UpnpDevice::forgetService: " << serviceId << endl);
    std::unique_lock<std::mutex> lock(m->devlock);

    std::unordered_map<string, UpnpService*>::iterator servit =
        m->servicemap.find(serviceId);
    if (servit != m->servicemap.end()) {
        m->servicemap.erase(servit);
    }
    vector<string>::iterator it =
        std::find(m->serviceids.begin(), m->serviceids.end(), serviceId);
    if (it != m->serviceids.end())
        m->serviceids.erase(it);
}

void UpnpDevice::addActionMapping(const UpnpService* serv,
                                  const std::string& actName,
                                  soapfun fun)
{
    std::unique_lock<std::mutex> lock(m->devlock);
    // LOGDEB("UpnpDevice::addActionMapping:" << actName << endl);
    m->calls[actName + serv->getServiceId()] = fun;
}

void UpnpDevice::Internal::notifyEvent(const string& serviceId,
                                       const vector<string>& names,
                                       const vector<string>& values)
{
//    LOGDEB1("UpnpDevice::notifyEvent " << serviceId << " " <<
//           (names.empty() ? "Empty names??" : names[0]) << endl);
    stringstream ss;
    for (unsigned int i = 0; i < names.size() && i < values.size(); i++) {
        ss << names[i] << "=" << values[i] << " ";
    }
    LOGDEB1("UpnpDevice::notifyEvent " << serviceId << " " <<
            (names.empty() ? "Empty names??" : ss.str()) << endl);
    if (names.empty())
        return;
    vector<const char *> cnames, cvalues;
    vector<string> qvalues;
    vectorstoargslists(names, values, qvalues, cnames, cvalues);

    int ret = UpnpNotify(dvh, deviceId.c_str(),
                         serviceId.c_str(), &cnames[0], &cvalues[0],
                         int(cnames.size()));
    if (ret != UPNP_E_SUCCESS) {
        LOGERR(lib->errAsString("UpnpDevice::notifyEvent: id", ret) <<
               " for " << serviceId << endl);
    }
}

int timespec_diffms(const struct timespec& old, const struct timespec& recent)
{
    return (recent.tv_sec - old.tv_sec) * 1000 +
           (recent.tv_nsec - old.tv_nsec) / (1000 * 1000);
}

// Loop on services, and poll each for changed data. Generate event
// only if changed data exists.
// We used to generate an artificial event with all the current state
// every 10 S or so. This does not seem to be necessary, and we do not
// do it any more.
// This is normally run by the main thread.
void UpnpDevice::eventloop()
{
    if (!m->start()) {
        LOGERR("Device would not start" << endl);
        return;
    }

    int count = 0;
    // Polling the services every 1 S
    const int loopwait_ms = 1000;
    // Full state every 10 S. This should not be necessary, but it
    // ensures that CPs get updated about our state even if they miss
    // some events. For example, the Songcast windows sender does not
    // see the TransportState transition to "Playing" if it is not
    // repeated few seconds later, with bad consequences on further
    // operations
    const int nloopstofull = 10;
    struct timespec wkuptime, earlytime;
    bool didearly = false;

    for (;;) {
        timespec_now(&wkuptime);
        timespec_addnanos(&wkuptime, loopwait_ms * 1000 * 1000);

        //LOGDEB("eventloop: now " << time(0) << " wkup at "<<
        //    wkuptime.tv_sec << " S " << wkuptime.tv_nsec << " ns" << endl);

        std::unique_lock<std::mutex> lock(m->evlooplock);
        std::cv_status err =
            m->evloopcond.wait_for(lock, chrono::milliseconds(loopwait_ms));
        if (m->needExit) {
            break;
        } else if (err == std::cv_status::no_timeout) {
            // Early wakeup. Only does something if it did not already
            // happen recently
            if (didearly) {
                int millis = timespec_diffms(earlytime, wkuptime);
                if (millis < loopwait_ms) {
                    // Do nothing. didearly stays true
                    // LOGDEB("eventloop: early, previous too close "<<endl);
                    continue;
                } else {
                    // had an early wakeup previously, but it was a
                    // long time ago. Update state and wakeup
                    // LOGDEB("eventloop: early, previous is old "<<endl);
                    earlytime = wkuptime;
                }
            } else {
                // Early wakeup, previous one was normal. Remember.
                // LOGDEB("eventloop: early, no previous" << endl);
                didearly = true;
                earlytime = wkuptime;
            }
        } else if (err == std::cv_status::timeout) {
            // Normal wakeup
            // LOGDEB("eventloop: normal wakeup" << endl);
            didearly = false;
        } else {
            LOGINF("UpnpDevice:eventloop: wait returned unexpected value" <<
                   int(err) << endl);
            break;
        }

        count++;
        bool all = count && ((count % nloopstofull) == 0);
        //LOGDEB("UpnpDevice::eventloop count "<<count<<" all "<<all<<endl);

        // We can't lock devlock around the loop because we don't want
        // to hold id when calling notifyEvent() (which calls
        // libupnp). This means that we should have a separate lock
        // for the services arrays. This would only be useful during
        // startup, while we add services, but the event loop is the
        // last call the main program will make after adding the
        // services, so locking does not seem necessary
        for (auto& it : m->serviceids) {
            vector<string> names, values;
            UpnpService* serv = m->servicemap[it];
            {

                // We don't generate periodic full event data any more
                // by default. Code kept around for easier switch back
                // on in case it's needed again
#ifndef LIBUPNPP_PERIODIC_FULL_DEVICE_EVENTS
                all = false;
#endif

                std::unique_lock<std::mutex> lock1(m->devlock);
                if (!serv->getEventData(all, names, values) || names.empty()) {
                    continue;
                }
            }
            if (!serv->noevents())
                m->notifyEvent(it, names, values);
        }
    }
}

// Can't take the loop lock here. We're called from the service and
// hold the device lock. The locks would be taken in opposite order,
// causing a potential deadlock:
//  - device action takes device lock
//  - loop wakes up, takes loop lock
//  - blocks on device lock before calling getevent
//  - device calls loopwakeup which blocks on loop lock
// -> deadlock
void UpnpDevice::loopWakeup()
{
    m->evloopcond.notify_all();
}

void UpnpDevice::shouldExit()
{
    m->needExit = true;
    m->evloopcond.notify_all();
}


class UpnpService::Internal {
public:
    Internal(bool noevs)
        : noevents(noevs), dev(0) {
    }
    bool noevents;
    UpnpDevice *dev;
};

UpnpService::UpnpService(const std::string& stp,
                         const std::string& sid, UpnpDevice *dev)
    : m_serviceType(stp), m_serviceId(sid), m(new Internal(false))
{
    m->dev = dev;
    dev->addService(this, sid);
}

UpnpService::UpnpService(const std::string& stp,
                         const std::string& sid, UpnpDevice *dev, bool noevs)
    : m_serviceType(stp), m_serviceId(sid), m(new Internal(noevs))
{
    m->dev = dev;
    dev->addService(this, sid);
}

UpnpDevice *UpnpService::getDevice()
{
    if (m) {
	return m->dev;
    } else {
	return nullptr;
    }
}

UpnpService::~UpnpService()
{
    if (m) {
        if (m->dev)
            m->dev->forgetService(m_serviceId);
        delete m;
        m = 0;
    }
}

bool UpnpService::noevents() const
{
    return m && m->noevents;
}

bool UpnpService::getEventData(bool, std::vector<std::string>&,
                               std::vector<std::string>&)
{
    return true;
}

const std::string& UpnpService::getServiceType() const
{
    return m_serviceType;
}

const std::string& UpnpService::getServiceId() const
{
    return m_serviceId;
}

const std::string UpnpService::errString(int error) const
{
    switch (error) {
    case UPNP_INVALID_ACTION: return "Invalid Action";
    case UPNP_INVALID_ARGS: return "Invalid Args";
    case UPNP_INVALID_VAR: return "Invalid Var";
    case UPNP_ACTION_FAILED: return "Action Failed";
    case UPNP_ARG_VALUE_INVALID: return "Arg Value Invalid";
    case UPNP_ARG_VALUE_OUT_OF_RANGE: return "Arg Value Out Of Range";
    case UPNP_OPTIONAL_ACTION_NOT_IMPLEMENTED:
        return "Optional Action Not Implemented";
    case UPNP_OUT_OF_MEMORY: return "Out Of MEMORY";
    case UPNP_HUMAN_INTERVENTION_REQUIRED: return "Human Intervention Required";
    case UPNP_STRING_ARGUMENT_TOO_LONG: return "String Argument Too Long";
    case UPNP_ACTION_NOT_AUTHORIZED: return "Action Not Authorized";
    case UPNP_SIGNATURE_FAILING: return "Signature Failing";
    case UPNP_SIGNATURE_MISSING: return "Signature Missing";
    case UPNP_NOT_ENCRYPTED: return "Not Encrypted";
    case UPNP_INVALID_SEQUENCE: return "Invalid Sequence";
    case UPNP_INVALID_CONTROL_URLS: return "Invalid Control URLS";
    case UPNP_NO_SUCH_SESSION: return "No Such Session";
    default:
        break;
    }
    return serviceErrString(error);
}

}// End namespace UPnPProvider
