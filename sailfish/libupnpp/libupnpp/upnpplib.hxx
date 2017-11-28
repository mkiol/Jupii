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
#ifndef _LIBUPNP_H_X_INCLUDED_
#define _LIBUPNP_H_X_INCLUDED_

#include <upnp/upnp.h>

#include <map>
#include <string>

namespace UPnPP {

/** Our link to libupnp. Initialize and keep the handle around.
 * 
 * Not all applications will need to use this. The initialization call will be
 * performed internally as needed, you only need to call it if you need to set 
 * specific parameters.
 */
class LibUPnP {
public:
    ~LibUPnP();

    /** Retrieve the singleton LibUPnP object
     *
     * This initializes libupnp, possibly setting an address and port, possibly
     * registering a client if serveronly is false.
     *
     * Note: libupnp uses a single v4 address/interface (and a v6 one if
     * enabled). This can be specified in the initialization call,
     * else it will pick the first non-loopback one. There is no way
     * to have a single instance of the library listen on multiple
     * interfaces.
     *
     * @param serveronly no client init
     * @param hwaddr returns the hardware address for the specified network
     *   interface, or the first one is ifname is empty. If the IP address is
     *   specified instead of the interface name, the hardware address
     *   returned is not necessarily the one matching the IP.
     * @param ifname if not empty, network interface to use. Translated to
     *   IP address for the UpnpInit() call.
     * @param ip if not empty, IP address to use. Only used if ifname is empty.
     * @param port   port parameter to UpnpInit() (0 for default).
     * @return 0 for failure.
     */
    static LibUPnP* getLibUPnP(bool serveronly = false, std::string* hwaddr = 0,
                               const std::string ifname = std::string(),
                               const std::string ip = std::string(),
                               unsigned short port = 0);

    /// Return the IP v4 address as dotted notation
    std::string host();
    
    /** Returns something like "libupnpp 0.14.0 libupnp x.y.z" */
    static std::string versionString();

    /** libupnp (pupnp) logging: this is distinct from libupnpp logging */
    /** libupnp log levels */
    enum LogLevel {LogLevelNone, LogLevelError, LogLevelDebug};
    /** Set libupnp log file name and activate/deactivate logging.
     *
     * This does nothing if libupnp was built with logging
     * disabled. It may be useful for debugging in some case. You
     * should not use the same file as the one used for libupnpp
     * logging.
     *
     * @param fn file name to use. Use empty string to turn logging off.
     */
    bool setLogFileName(const std::string& fn, LogLevel level = LogLevelError);
    /** Set libupnp log level. 
     *
     * @return true if libupnp logging is enabled, else false.
     */
    bool setLogLevel(LogLevel level);

    /** Set max library buffer size for reading content from servers. 
     * 
     * Just calls libupnp UpnpSetMaxContentLength(). This defines the maximum
     * amount of data that Soap calls will accept from the remote. The
     * libupnp default is very low (16KB), but libupnpp sets the value
     * to 2MB during initialization. You can reset the value to your
     * own choice by using this method.
     */
    void setMaxContentLength(int bytes);

    /** Check state after initialization */
    bool ok() const;

    /** Retrieve init error if state not ok */
    int getInitError() const;

    /** Build a unique persistent UUID for a root device. This uses a hash
        of the input name (e.g.: friendlyName), and the host Ethernet address */
    static std::string makeDevUUID(const std::string& name,
                                   const std::string& hw);

    /** Translate libupnp integer error code (UPNP_E_XXX) to string */
    static std::string errAsString(const std::string& who, int code);

/////////////////////////////////////////////////////////////////////////////
    /* The methods which follow are normally for use by the
     * intermediate layers in libupnpp, such as the base device class
     * or the server directory, end-user code should not need them in
     * general.
     */

    /** Private: specify function to be called on given UPnP
     *  event. This will happen in the libupnp thread context.
     */
    void registerHandler(Upnp_EventType et, Upnp_FunPtr handler, void *cookie);

    /** Private: translate libupnp event type to string */
    static std::string evTypeAsString(Upnp_EventType);

    int setupWebServer(const std::string& description, UpnpDevice_Handle *dvh);

    UpnpClient_Handle getclh();

private:
    class Internal;
    Internal *m;

    LibUPnP(bool serveronly, std::string *hwaddr,
            const std::string ifname, const std::string ip,
            unsigned short port);
    LibUPnP(const LibUPnP &);
    LibUPnP& operator=(const LibUPnP &);

    static int o_callback(Upnp_EventType, void *, void *);
};

} // namespace UPnPP

#endif /* _LIBUPNP.H_X_INCLUDED_ */
