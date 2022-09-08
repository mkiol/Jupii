/*******************************************************************************
 *
 * Copyright (c) 2020 Jean-Francois Dockes
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions are met: 
 *
 * - Redistributions of source code must retain the above copyright notice, 
 * this list of conditions and the following disclaimer. 
 * - Redistributions in binary form must reproduce the above copyright notice, 
 * this list of conditions and the following disclaimer in the documentation 
 * and/or other materials provided with the distribution. 
 * - Neither name of Intel Corporation nor the names of its contributors 
 * may be used to endorse or promote products derived from this software 
 * without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR 
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL INTEL OR 
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, 
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, 
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR 
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY 
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS 
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 ******************************************************************************/
#ifndef _NETIF_H_INCLUDED_
#define _NETIF_H_INCLUDED_

/** @file netif.h
 *
 * @brief Implement a simplified and system-idependant interface to a
 * system's network interfaces.
 */
#include <stdio.h>
#include <memory>
#include <string>
#include <ostream>
#include <vector>
#ifdef _WIN32
#include <winsock2.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#endif

#include "UpnpGlobal.h"

namespace NetIF {

/** @brief Represent an IPV4 or IPV6 address */
class EXPORT_SPEC IPAddr {
public:
    /** @brief address family types. The values are identifal to the
     * system's definitions */
    enum class Family {Invalid = -1, IPV4 = AF_INET, IPV6 = AF_INET6};
    /** @brief IPV6 address scope type */
    enum class Scope {Invalid = -1, LINK, SITE, GLOBAL};
    IPAddr();
    /** @brief Build from textual representation (e.g. 192.168.4.4) */
    explicit IPAddr(const char *);
    /** @brief Build from textual representation (e.g. 192.168.4.4) */
    explicit IPAddr(const std::string& s)
        : IPAddr(s.c_str()) {}
    /** @brief Build from binary address in network byte order. 
     *  @param unmapv4 if true we test for a v6 mapped v4 address and, if found, store it as v4 
     */
    explicit IPAddr(const struct sockaddr *sa, bool unmapv4=true);

    IPAddr(const IPAddr&);
    IPAddr& operator=(const IPAddr&);
    ~IPAddr();

    /** @brief Check constructor success */
    bool ok() const;
    /** Set the scopeidx from other address. Only does anything for ipv6 link-local addresses */
    bool setScopeIdx(const IPAddr& other);
    /** @brief Returns the address family */
    Family family() const;
    /** @brief Returns the scope type of IPV6 address */
    Scope scopetype() const;
    /** @brief Copies out for use with a system interface
     * Zeroes out up to sizeof(sockaddr_storage) */
    bool copyToStorage(struct sockaddr_storage *dest) const;
    /** @brief Copies out for use with a system interface
     * Copies exactly the needed size */
    bool copyToAddr(struct sockaddr *dest) const;

    /** @brief Get reference to the internal binary address */
    const struct sockaddr_storage& getaddr() const;
    
    /** @brief Convert to textual representation. */
    std::string straddr() const;
    /** @brief Convert to textual representation. 
     * Possibly add scope id, possibly url-encode it. */
    std::string straddr(bool setscope, bool forurl) const;
    
    friend class Interface;
    class Internal;
private:
    std::unique_ptr<Internal> m;
};

/** @brief Represent a system network interface, its attributes and its
 * addresses. Usually built by the module internal code by
 * querying the system interfaces. */
class EXPORT_SPEC Interface {
public:
    /** @brief Interface attributes */
    enum class Flags {NONE = 0, HASIPV4 = 1, HASIPV6 = 2, LOOPBACK=4,
                      UP=8, MULTICAST=0x10, HASHWADDR=0x20};
    Interface();
    /** @brief Construct empty interface with just the name set */
    Interface(const char *nm);
    /** @brief Construct empty interface with just the name set */
    Interface(const std::string &nm);
    ~Interface();
    Interface(const Interface&);
    Interface& operator=(const Interface&);

    /** @brief Return the interface name */
    const std::string& getname() const;
    /** @brief Return the interface friendly name (same as name except
     * on Windows). */
    const std::string& getfriendlyname() const;
    
    /** Return the hardware (ethernet) address as a binary string (can have 
     *  embedded null characters). Empty if no hardware address was
     *  found for this interface */
    const std::string& gethwaddr() const;
    /** @brief Return hardware address in traditional colon-separated hex */
    std::string gethexhwaddr() const;
    /** @brief test if flag is set */
    bool hasflag(Flags f) const;
    /** @brief Remove all addresses not in the input vector */
    bool trimto(const std::vector<IPAddr>& keep);
    /** @brief Return the first ipv4 address if any, or nullptr */
    const IPAddr *firstipv4addr() const;
    /** @brief Return the first ipv6 address if any, or nullptr */
    const IPAddr *firstipv6addr(
        IPAddr::Scope scope = IPAddr::Scope::Invalid) const;
    /** @brief Return the interface addresses and the corresponding
     * netmasks, as parallel arrays */
    std::pair<const std::vector<IPAddr>&, const std::vector<IPAddr>&>
    getaddresses() const;
    /** @brief Return the interface index */
    int getindex() const;
    
    /** @brief Print out, a bit like "ip addr" output */
    std::ostream& print(std::ostream&) const;

    class Internal;
    friend class Interfaces;
private:
    std::unique_ptr<Internal> m;
};


/** @brief Represent the system's network interfaces. Singleton class. */
class EXPORT_SPEC Interfaces {
public:
    /** @brief Return the Interfaces singleton after possibly building
     * it by querying the system */
    static Interfaces *theInterfaces();

    /** @brief Read the state from the system again */
    bool refresh();
    
    /** @brief Find interface by name or friendlyname */
    Interface *findByName(const char*nm) const;
    /** @brief Find interface by name or friendlyname */
    Interface *findByName(const std::string& nm) const{
        return findByName(nm.c_str());
    }

    /** @brief Argument to the select() method: flags which we want or don't */
    struct Filter {
        /** @brief flags we want. */
        std::vector<Interface::Flags> needs;
        /** @brief flags we don't want.*/
        std::vector<Interface::Flags> rejects;
    };

    /** @brief Return Interface objects satisfying the criteria in f. */
    std::vector<Interface> select(const Filter& f) const;
    
    /** @brief Print out, a bit like "ip addr" output */
    std::ostream& print(std::ostream&);

    /** @brief Find the interface to which the input address' subnet
     * belongs to in a vector of @ref Interface.
     * @param addr the address we're looking for
     * @param vifs the interfaces to search
     * @param[out] hostaddr the found interface address.
     * @return both the interface and the address inside the interface.
     */
    static const Interface *interfaceForAddress(
        const IPAddr& addr, const std::vector<Interface>& vifs,IPAddr& hostaddr);

    /** @brief Find the interface to which the input address' subnet
     * belongs to among all the system interfaces.
     * @param addr the address we're looking for
     * @param[out] hostaddr the found interface address.
     * @return Both the interface and the address inside the interface. */
    const Interface *interfaceForAddress(const IPAddr& addr, IPAddr& hostaddr);

    /** debug */
    static void setlogfp(FILE *fp);
    
private:
    Interfaces(const Interfaces &) = delete;
    Interfaces& operator=(const Interfaces &) = delete;
    Interfaces();
    ~Interfaces();

    class Internal;
    std::unique_ptr<Internal> m;
};

} /* namespace NetIF */

#endif /* _NETIF_H_INCLUDED_ */
