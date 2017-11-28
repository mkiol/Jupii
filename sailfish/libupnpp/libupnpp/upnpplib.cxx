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
#define  LIBUPNPP_NEED_PACKAGE_VERSION
#include "libupnpp/config.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#ifdef __MACH__
#include <mach/clock.h>
#include <mach/mach.h>
#endif

#include <ixml.h>
#include <upnp/upnptools.h>
#include <upnp/upnpdebug.h>

#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "libupnpp/getsyshwaddr.h"
#include "libupnpp/log.hxx"
#include "libupnpp/md5.hxx"
#include "libupnpp/upnpp_p.hxx"
#include "libupnpp/upnpplib.hxx"
#include "libupnpp/upnpputils.hxx"

using namespace std;

namespace UPnPP {

static LibUPnP *theLib;

class LibUPnP::Internal {
public:
    // A Handler object records the data from registerHandler.
    class Handler {
    public:
        Handler()
            : handler(0), cookie(0) {}
        Handler(Upnp_FunPtr h, void *c)
            : handler(h), cookie(c) {}
        Upnp_FunPtr handler;
        void *cookie;
    };

    bool ok;
    int  init_error;
    UpnpClient_Handle clh;
    std::mutex mutex;
    std::map<Upnp_EventType, Handler> handlers;
};

LibUPnP *LibUPnP::getLibUPnP(bool serveronly, string* hwaddr,
                             const string ifname, const string ip,
                             unsigned short port)
{
    if (theLib && !theLib->ok()) {
        delete theLib;
        theLib = 0;
    }

    if (theLib == 0)
        theLib = new LibUPnP(serveronly, hwaddr, ifname, ip, port);
    if (theLib && !theLib->ok()) {
        delete theLib;
        theLib = 0;
        return 0;
    }
    return theLib;
}

string LibUPnP::versionString()
{
    // We would like to print libupnp version too, but there is no way to
    // get this at runtime, and printing the compile time version is wrong.
    return string("libupnpp ") + LIBUPNPP_PACKAGE_VERSION;
}

UpnpClient_Handle LibUPnP::getclh()
{
    return m->clh;
}

bool LibUPnP::ok() const
{
    return m->ok;
}

int LibUPnP::getInitError() const
{
    return m->init_error;
}

LibUPnP::LibUPnP(bool serveronly, string* hwaddr,
                 const string ifname, const string inip, unsigned short port)
{
    LOGDEB1("LibUPnP: serveronly " << serveronly << " &hwaddr " << hwaddr <<
            " ifname [" << ifname << "] inip [" << inip << "] port " << port
            << endl);

    if ((m = new Internal()) == 0) {
        LOGERR("LibUPnP::LibUPnP: out of memory" << endl);
        return;
    }

    m->ok = false;

    // If our caller wants to retrieve an ethernet address (typically
    // for uuid purposes), or has specified an interface we have to
    // look at the network config.
    const int ipalen(100);
    char ip_address[ipalen];
    ip_address[0] = 0;
    if (hwaddr || !ifname.empty()) {
        char mac[20];
        if (getsyshwaddr(ifname.c_str(), ip_address, ipalen, mac, 13,0,0) < 0) {
            LOGERR("LibUPnP::LibUPnP: failed retrieving addr" << endl);
            return;
        }
        if (hwaddr)
            *hwaddr = string(mac);
    }

    // If the interface name was not specified, we possibly use the
    // supplied IP address. If this is empty too, libupnp will choose
    // by itself (the first adapter).
    if (ifname.empty())
        strncpy(ip_address, inip.c_str(), ipalen);

    m->init_error = UpnpInit(ip_address[0] ? ip_address : 0, port);

    if (m->init_error != UPNP_E_SUCCESS) {
        LOGERR(errAsString("UpnpInit", m->init_error) << endl);
        return;
    }
    setMaxContentLength(2000*1024);

    LOGDEB("LibUPnP: Using IP " << UpnpGetServerIpAddress() << " port " <<
           UpnpGetServerPort() << endl);

#if defined(HAVE_UPNPSETLOGLEVEL)
    UpnpCloseLog();
#endif

    // Client initialization is simple, just do it. Defer device
    // initialization because it's more complicated.
    if (serveronly) {
        m->ok = true;
    } else {
        m->init_error = UpnpRegisterClient(o_callback, (void *)this, &m->clh);

        if (m->init_error == UPNP_E_SUCCESS) {
            m->ok = true;
        } else {
            LOGERR(errAsString("UpnpRegisterClient", m->init_error) << endl);
        }
    }

    // Servers sometimes make errors (e.g.: minidlna returns bad utf-8).
    ixmlRelaxParser(1);
}

string LibUPnP::host()
{
    char *cp = UpnpGetServerIpAddress();
    if (cp)
        return cp;
    return string();
}

int LibUPnP::setupWebServer(const string& description, UpnpDevice_Handle *dvh)
{
    int res = UpnpRegisterRootDevice2(
                  UPNPREG_BUF_DESC,
                  description.c_str(),
                  description.size(), /* Desc filename len, ignored */
                  1, /* config_baseURL */
                  o_callback, (void *)this, dvh);

    if (res != UPNP_E_SUCCESS) {
        LOGERR(errAsString("UpnpRegisterRootDevice2", res) << " description " <<
               description << endl);
    }
    return res;
}

void LibUPnP::setMaxContentLength(int bytes)
{
    UpnpSetMaxContentLength(size_t(bytes));
}

bool LibUPnP::setLogFileName(const std::string& fn, LogLevel level)
{
    std::unique_lock<std::mutex> lock(m->mutex);
#if defined(HAVE_UPNPSETLOGLEVEL)
    if (fn.empty() || level == LogLevelNone) {
        UpnpCloseLog();
    } else {
        setLogLevel(level);
        UpnpSetLogFileNames(fn.c_str(), fn.c_str());
        int code = UpnpInitLog();
        if (code != UPNP_E_SUCCESS) {
            LOGERR(errAsString("UpnpInitLog", code) << endl);
            return false;
        }
    }
    return true;
#else
    return false;
#endif
}

bool LibUPnP::setLogLevel(LogLevel level)
{
#if defined(HAVE_UPNPSETLOGLEVEL)
    switch (level) {
    case LogLevelNone:
        setLogFileName("", LogLevelNone);
        break;
    case LogLevelError:
        UpnpSetLogLevel(UPNP_CRITICAL);
        break;
    case LogLevelDebug:
        UpnpSetLogLevel(UPNP_ALL);
        break;
    }
    return true;
#else
    return false;
#endif
}

void LibUPnP::registerHandler(Upnp_EventType et, Upnp_FunPtr handler,
                              void *cookie)
{
    std::unique_lock<std::mutex> lock(m->mutex);
    if (handler == 0) {
        m->handlers.erase(et);
    } else {
        Internal::Handler h(handler, cookie);
        m->handlers[et] = h;
    }
}

std::string LibUPnP::errAsString(const std::string& who, int code)
{
    std::ostringstream os;
    os << who << " :" << code << ": " << UpnpGetErrorMessage(code);
    return os.str();
}

int LibUPnP::o_callback(Upnp_EventType et, void* evp, void* cookie)
{
    LibUPnP *ulib = (LibUPnP *)cookie;
    if (ulib == 0) {
        // Because the asyncsearch calls uses a null cookie.
        //cerr << "o_callback: NULL ulib!" << endl;
        ulib = theLib;
    }
    LOGDEB1("LibUPnP::o_callback: event type: " << evTypeAsString(et) << endl);

    map<Upnp_EventType, Internal::Handler>::iterator it = ulib->m->handlers.find(et);
    if (it != ulib->m->handlers.end()) {
        (it->second.handler)(et, evp, it->second.cookie);
    }
    return UPNP_E_SUCCESS;
}

LibUPnP::~LibUPnP()
{
    int error = UpnpFinish();
    if (error != UPNP_E_SUCCESS) {
        LOGINF("LibUPnP::~LibUPnP: " << errAsString("UpnpFinish", error)
               << endl);
    }
    delete m;
    m = 0;
    LOGDEB1("LibUPnP: done" << endl);
}

string LibUPnP::makeDevUUID(const std::string& name, const std::string& hw)
{
    string digest;
    MD5String(name, digest);
    // digest has 16 bytes of binary data. UUID is like:
    // f81d4fae-7dec-11d0-a765-00a0c91e6bf6
    // Where the last 12 chars are provided by the hw addr

    char uuid[100];
    sprintf(uuid, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%s",
            digest[0]&0xff, digest[1]&0xff, digest[2]&0xff, digest[3]&0xff,
            digest[4]&0xff, digest[5]&0xff,  digest[6]&0xff, digest[7]&0xff,
            digest[8]&0xff, digest[9]&0xff, hw.c_str());
    return uuid;
}


string LibUPnP::evTypeAsString(Upnp_EventType et)
{
    switch (et) {
    case UPNP_CONTROL_ACTION_REQUEST:
        return "UPNP_CONTROL_ACTION_REQUEST";
    case UPNP_CONTROL_ACTION_COMPLETE:
        return "UPNP_CONTROL_ACTION_COMPLETE";
    case UPNP_CONTROL_GET_VAR_REQUEST:
        return "UPNP_CONTROL_GET_VAR_REQUEST";
    case UPNP_CONTROL_GET_VAR_COMPLETE:
        return "UPNP_CONTROL_GET_VAR_COMPLETE";
    case UPNP_DISCOVERY_ADVERTISEMENT_ALIVE:
        return "UPNP_DISCOVERY_ADVERTISEMENT_ALIVE";
    case UPNP_DISCOVERY_ADVERTISEMENT_BYEBYE:
        return "UPNP_DISCOVERY_ADVERTISEMENT_BYEBYE";
    case UPNP_DISCOVERY_SEARCH_RESULT:
        return "UPNP_DISCOVERY_SEARCH_RESULT";
    case UPNP_DISCOVERY_SEARCH_TIMEOUT:
        return "UPNP_DISCOVERY_SEARCH_TIMEOUT";
    case UPNP_EVENT_SUBSCRIPTION_REQUEST:
        return "UPNP_EVENT_SUBSCRIPTION_REQUEST";
    case UPNP_EVENT_RECEIVED:
        return "UPNP_EVENT_RECEIVED";
    case UPNP_EVENT_RENEWAL_COMPLETE:
        return "UPNP_EVENT_RENEWAL_COMPLETE";
    case UPNP_EVENT_SUBSCRIBE_COMPLETE:
        return "UPNP_EVENT_SUBSCRIBE_COMPLETE";
    case UPNP_EVENT_UNSUBSCRIBE_COMPLETE:
        return "UPNP_EVENT_UNSUBSCRIBE_COMPLETE";
    case UPNP_EVENT_AUTORENEWAL_FAILED:
        return "UPNP_EVENT_AUTORENEWAL_FAILED";
    case UPNP_EVENT_SUBSCRIPTION_EXPIRED:
        return "UPNP_EVENT_SUBSCRIPTION_EXPIRED";
    default:
        return "UPNP UNKNOWN EVENT";
    }
}

/////////////////////// Small global helpers

/** Get rid of white space at both ends */
void trimstring(string &s, const char *ws)
{
    string::size_type pos = s.find_first_not_of(ws);
    if (pos == string::npos) {
        s.clear();
        return;
    }
    s.replace(0, pos, string());

    pos = s.find_last_not_of(ws);
    if (pos != string::npos && pos != s.length()-1)
        s.replace(pos+1, string::npos, string());
}

string caturl(const string& s1, const string& s2)
{
    string out(s1);
    if (out[out.size()-1] == '/') {
        if (s2[0] == '/')
            out.erase(out.size()-1);
    } else {
        if (s2[0] != '/')
            out.push_back('/');
    }
    out += s2;
    return out;
}

string baseurl(const string& url)
{
    string::size_type pos = url.find("://");
    if (pos == string::npos)
        return url;

    pos = url.find_first_of("/", pos + 3);
    if (pos == string::npos) {
        return url;
    } else {
        return url.substr(0, pos + 1);
    }
}

static void path_catslash(string &s) {
    if (s.empty() || s[s.length() - 1] != '/')
        s += '/';
}
string path_getfather(const string &s)
{
    string father = s;

    // ??
    if (father.empty())
        return "./";

    if (father[father.length() - 1] == '/') {
        // Input ends with /. Strip it, handle special case for root
        if (father.length() == 1)
            return father;
        father.erase(father.length()-1);
    }

    string::size_type slp = father.rfind('/');
    if (slp == string::npos)
        return "./";

    father.erase(slp);
    path_catslash(father);
    return father;
}
string path_getsimple(const string &s) {
    string simple = s;

    if (simple.empty())
        return simple;

    string::size_type slp = simple.rfind('/');
    if (slp == string::npos)
        return simple;

    simple.erase(0, slp+1);
    return simple;
}

template <class T> bool csvToStrings(const string &s, T &tokens)
{
    string current;
    tokens.clear();
    enum states {TOKEN, ESCAPE};
    states state = TOKEN;
    for (unsigned int i = 0; i < s.length(); i++) {
        switch (s[i]) {
        case ',':
            switch(state) {
            case TOKEN:
                tokens.insert(tokens.end(), current);
                current.clear();
                continue;
            case ESCAPE:
                current += ',';
                state = TOKEN;
                continue;
            }
            break;
        case '\\':
            switch(state) {
            case TOKEN:
                state=ESCAPE;
                continue;
            case ESCAPE:
                current += '\\';
                state = TOKEN;
                continue;
            }
            break;

        default:
            switch(state) {
            case ESCAPE:
                state = TOKEN;
                break;
            case TOKEN:
                break;
            }
            current += s[i];
        }
    }
    switch(state) {
    case TOKEN:
        tokens.insert(tokens.end(), current);
        break;
    case ESCAPE:
        return false;
    }
    return true;
}

//template bool csvToStrings<list<string> >(const string &, list<string> &);
template bool csvToStrings<vector<string> >(const string &, vector<string> &);
template bool csvToStrings<set<string> >(const string &, set<string> &);


bool stringToBool(const string& s, bool *value)
{
    if (s[0] == 'F' ||s[0] == 'f' ||s[0] == 'N' || s[0] == 'n' ||s[0] == '0') {
        *value = false;
    } else if (s[0] == 'T'|| s[0] == 't' ||s[0] == 'Y' ||s[0] == 'y' ||
               s[0] == '1') {
        *value = true;
    } else {
        return false;
    }
    return true;
}

//  s1 is already uppercase
int stringuppercmp(const string & s1, const string& s2)
{
    string::const_iterator it1 = s1.begin();
    string::const_iterator it2 = s2.begin();
    string::size_type size1 = s1.length(), size2 = s2.length();
    int c2;

    if (size1 > size2) {
        while (it1 != s1.end()) {
            c2 = ::toupper(*it2);
            if (*it1 != c2) {
                return *it1 > c2 ? 1 : -1;
            }
            ++it1;
            ++it2;
        }
        return size1 == size2 ? 0 : 1;
    } else {
        while (it2 != s2.end()) {
            c2 = ::toupper(*it2);
            if (*it1 != c2) {
                return *it1 > c2 ? 1 : -1;
            }
            ++it1;
            ++it2;
        }
        return size1 == size2 ? 0 : -1;
    }
}

#ifdef _MSC_VER
// Note: struct timespec is defined by pthread.h (from pthreads-w32)
#ifndef CLOCK_REALTIME
#define CLOCK_REALTIME 0
#endif

LARGE_INTEGER getFILETIMEoffset()
{
    SYSTEMTIME s;
    FILETIME f;
    LARGE_INTEGER t;

    s.wYear = 1970;
    s.wMonth = 1;
    s.wDay = 1;
    s.wHour = 0;
    s.wMinute = 0;
    s.wSecond = 0;
    s.wMilliseconds = 0;
    SystemTimeToFileTime(&s, &f);
    t.QuadPart = f.dwHighDateTime;
    t.QuadPart <<= 32;
    t.QuadPart |= f.dwLowDateTime;
    return (t);
}

int clock_gettime(int X, struct timespec *tv)
{
    LARGE_INTEGER           t;
    FILETIME            f;
    double                  microseconds;
    static LARGE_INTEGER    offset;
    static double           frequencyToMicroseconds;
    static int              initialized = 0;
    static BOOL             usePerformanceCounter = 0;

    if (!initialized) {
        LARGE_INTEGER performanceFrequency;
        initialized = 1;
        usePerformanceCounter = QueryPerformanceFrequency(&performanceFrequency);
        if (usePerformanceCounter) {
            QueryPerformanceCounter(&offset);
            frequencyToMicroseconds = (double)performanceFrequency.QuadPart / 1000000.;
        }
        else {
            offset = getFILETIMEoffset();
            frequencyToMicroseconds = 10.;
        }
    }
    if (usePerformanceCounter) QueryPerformanceCounter(&t);
    else {
        GetSystemTimeAsFileTime(&f);
        t.QuadPart = f.dwHighDateTime;
        t.QuadPart <<= 32;
        t.QuadPart |= f.dwLowDateTime;
    }

    t.QuadPart -= offset.QuadPart;
    microseconds = (double)t.QuadPart / frequencyToMicroseconds;
    t.QuadPart = (long long)microseconds;
    tv->tv_sec = t.QuadPart / 1000000;
    tv->tv_nsec = (t.QuadPart % 1000000) * 1000;
    return (0);
}
#endif

void timespec_now(struct timespec *tsp)
{
#ifdef __MACH__ // Mac OS X does not have clock_gettime, use clock_get_time
    clock_serv_t cclock;
    mach_timespec_t mts;
    host_get_clock_service(mach_host_self(), CALENDAR_CLOCK, &cclock);
    clock_get_time(cclock, &mts);
    mach_port_deallocate(mach_task_self(), cclock);
    tsp->tv_sec = mts.tv_sec;
    tsp->tv_nsec = mts.tv_nsec;
#else
    clock_gettime(CLOCK_REALTIME, tsp);
#endif
}


static const int BILLION = 1000 * 1000 * 1000;

void timespec_addnanos(struct timespec *ts, long long nanos)
{
    nanos = nanos + ts->tv_nsec;
    long long secs = 0;
    if (nanos > BILLION) {
        secs = nanos / BILLION;
        nanos -= secs * BILLION;
    }
    ts->tv_sec += secs;
    ts->tv_nsec = long(nanos);
}

static void takeadapt(void *tok, const char *name)
{
    vector<string>* ads = (vector<string>*)tok;
    ads->push_back(name);
}
bool getAdapterNames(vector<string>& names)
{
    return getsyshwaddr("!?#@", 0, 0, 0, 0, takeadapt, &names) == 0;
}

}
