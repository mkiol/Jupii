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
#ifndef _UPNPPDISC_H_X_INCLUDED_
#define _UPNPPDISC_H_X_INCLUDED_

#include <time.h>

#include <string>
#include <functional>
#include <unordered_map>

#include "libupnpp/upnppexports.hxx"

namespace UPnPClient {
class UPnPDeviceDesc;
}
namespace UPnPClient {
class UPnPServiceDesc;
}

namespace UPnPClient {

/**
 * Manage UPnP discovery and maintain a directory of active devices. Singleton.
 *
 *
 * The service is initialized on the first call, starting
 * the message-handling thread, registering our message handlers,
 * and initiating an asynchronous UPnP device search.
 *
 * The search implies a timeout period (the specified interval
 * over which the servers will send replies at random points). Any
 * subsequent traverse() call will block until the timeout
 * is expired. Use getRemainingDelayMs() to know the current
 * remaining delay, and use it to do something else.
 *
 * We need a separate thread to process the messages coming up
 * from libupnp, because some of them will in turn trigger other
 * calls to libupnp, and this must not be done from the libupnp
 * thread context which reported the initial message.
 * So there are three threads in action:
 *  - The reporting thread from libupnp.
 *  - The discovery service processing thread, which also runs the callbacks.
 *  - The user thread (typically the main thread), which calls traverse.
 */
class UPNPP_API UPnPDeviceDirectory {
public:
    /** Retrieve the singleton object for the discovery service,
     * and possibly start it up if this is the first call. This does 
     * not wait significantly: a subsequent traverse() will wait until 
     * the initial delay is consumed. 2 S is libupnp MIN_SEARCH_WAIT, 
     * I don't see much reason to use more
     */
    static UPnPDeviceDirectory *getTheDir(time_t search_window = 2);

    /** Clean up before exit. Do call this.*/
    static void terminate();

    /** Type of user callback functions used for reporting devices and
     * services */
    typedef std::function<bool (const UPnPDeviceDesc&,
                                const UPnPServiceDesc&)> Visitor;

    /** Possibly wait for the end of the initial delay, then traverse
     * the directory and call Visitor for each device/service pair */
    bool traverse(Visitor);

    /** Remaining milliseconds until current search complete. */
    time_t getRemainingDelayMs();
    /** Remaining seconds until current search complete. Better use
     * getRemainingDelayMs(), this is kept only for compatibility. */
    time_t getRemainingDelay();

    /** Set a callback to be called when devices report their existence
     *  The function will be called once per device, with an empty service,
     *  Note that calls to v may be performed from a separate thread
     *  and some may occur before addCallback() returns.
     */
    static unsigned int addCallback(Visitor v);
    static void delCallback(unsigned int idx);

    /** Find device by 'friendly name'.
     *
     * This will wait for the remaining duration of the search window if the 
     * device is not found at once.
     * Note that "friendly names" are not necessarily unique. The method will
     * return a random instance (the first found) if there are several.
     * @param fname the device UPnP "friendly name" to be looked for
     * @param[out] ddesc the description data if the device was found.
     * @return true if the name was found, else false.
     */
    bool getDevByFName(const std::string& fname, UPnPDeviceDesc& ddesc);

    /** Find device by UDN.
     *
     * This will wait for the remaining duration of the search window if the 
     * device is not found at once.
     * @param udn the device Unique Device Name, a UUID.
     * @param[out] ddesc the description data if the device was found.
     * @return true if the device was found, else false.
     */
    bool getDevByUDN(const std::string& udn, UPnPDeviceDesc& ddesc);

    /** Helper function: retrieve all description data for a  named device 
     *  @param uidOrFriendly device identification. First tried as UUID then 
     *      friendly name.
     *  @param[output] deviceXML device description document.
     *  @param[output] srvsXML service name - service description map.
     *  @return true for success.
     */
    bool getDescriptionDocuments(
        const std::string &uidOrFriendly, std::string& deviceXML,
        std::unordered_map<std::string, std::string>& srvsXML);

    
    /** My health */
    bool ok();

    /** My diagnostic if health is bad */
    const std::string getReason();

private:
    UPnPDeviceDirectory(const UPnPDeviceDirectory &) = delete;
    UPnPDeviceDirectory& operator=(const UPnPDeviceDirectory &) = delete;
    UPNPP_LOCAL UPnPDeviceDirectory(time_t search_window);
};

} // namespace UPnPClient

#endif /* _UPNPPDISC_H_X_INCLUDED_ */
