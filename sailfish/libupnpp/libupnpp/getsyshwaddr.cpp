/* MiniUPnP project
 * http://miniupnp.free.fr/ or http://miniupnp.tuxfamily.org/
 *
 * Copyright (c) 2006, Thomas Bernard
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * The name of the author may not be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#include "libupnpp/config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef WIN32
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>

#if defined(sun)
#include <sys/sockio.h>
#endif

#if HAVE_GETIFADDRS
#include <ifaddrs.h>
#ifdef __linux__
#ifndef AF_LINK
#define AF_LINK AF_INET
#endif
#else
#include <net/if_dl.h>
#endif
#endif

#else /* WIN32-> */
// Needs iphlpapi.lib
#include <winsock2.h>
#include <Windows.h>
#include <Iphlpapi.h>
#endif

#include "libupnpp/getsyshwaddr.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MACADDR_IS_ZERO(x)                      \
    ((x[0] == 0x00) &&                          \
     (x[1] == 0x00) &&                          \
     (x[2] == 0x00) &&                          \
     (x[3] == 0x00) &&                          \
     (x[4] == 0x00) &&                          \
     (x[5] == 0x00))

#define DPRINTF(A,B,C,D) 

int getsyshwaddr(const char *iface, char *ip, int ilen, char *buf, int hlen,
                 type_ifreporter ifreporter, void *tok)
{
    unsigned char mac[6];
    int ret = -1;
    int ifnamelen = 0;

    if (iface && *iface) {
        ifnamelen = strnlen(iface, 200);
    }
//	fprintf(stderr, "getsyshwaddr: [%s](%d), ilen %d hlen %d, ifreporter %p\n",
//			iface, ifnamelen, ilen, hlen, ifreporter);

    
    memset(&mac, 0, sizeof(mac));

#ifndef WIN32
#if HAVE_GETIFADDRS
    struct ifaddrs *ifap, *p;

    if (getifaddrs(&ifap) != 0)
    {
        DPRINTF(E_ERROR, L_GENERAL, "getifaddrs(): %s\n", strerror(errno));
        return -1;
    }
    for (p = ifap; p != NULL; p = p->ifa_next)
    {
        if (p->ifa_addr && p->ifa_addr->sa_family == AF_LINK)
        {
            struct sockaddr_in *addr_in;
            uint8_t a;

            if (ifreporter && strcmp(p->ifa_name , "lo")) {
                ifreporter(tok, p->ifa_name);
            }

            if (ifnamelen && strcmp(iface, p->ifa_name))
                continue;

            addr_in = (struct sockaddr_in *)p->ifa_addr;
            a = (htonl(addr_in->sin_addr.s_addr) >> 0x18) & 0xFF;
            if (a == 127)
                continue;

            if (ip)
                inet_ntop(AF_INET, (const void *) &(addr_in->sin_addr), 
                          ip, ilen);

#ifdef __linux__
            struct ifreq ifr;
            int fd;
            fd = socket(AF_INET, SOCK_DGRAM, 0);
            if (fd < 0)
                continue;
            strncpy(ifr.ifr_name, p->ifa_name, IFNAMSIZ);
            if (ioctl(fd, SIOCGIFHWADDR, &ifr) < 0)
            {
                close(fd);
                continue;
            }
            memcpy(mac, ifr.ifr_hwaddr.sa_data, 6);
            close(fd);
#else  /* ! __linux__ -> */
            struct sockaddr_dl *sdl;
            sdl = (struct sockaddr_dl*)p->ifa_addr;
            memcpy(mac, LLADDR(sdl), sdl->sdl_alen);
#endif
            if (MACADDR_IS_ZERO(mac))
                continue;
            ret = 0;
            break;
        }
    }
    freeifaddrs(ifap);

#else /* ! HAVE_GETIFADDRS -> */

    struct if_nameindex *ifaces, *if_idx;
    struct ifreq ifr;
    int fd;

    /* Get the spatially unique node identifier */
    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0)
        return ret;

    ifaces = if_nameindex();
    if (!ifaces)
        return ret;

    for (if_idx = ifaces; if_idx->if_index; if_idx++)
    {
        if (ifreporter && strcmp(if_idx->if_name , "lo")) {
            ifreporter(tok, if_idx->if_name);
        }
        if (ifnamelen && strcmp(name, if_idx->if_name))
            continue;

        strncpy(ifr.ifr_name, if_idx->if_name, IFNAMSIZ);
        if (ioctl(fd, SIOCGIFFLAGS, &ifr) < 0)
            continue;
        if (ifr.ifr_ifru.ifru_flags & IFF_LOOPBACK)
            continue;
        if (ioctl(fd, SIOCGIFHWADDR, &ifr) < 0)
            continue;
        if (MACADDR_IS_ZERO(ifr.ifr_hwaddr.sa_data))
            continue;
        memcpy(mac, ifr.ifr_hwaddr.sa_data, 6);
        if (ip) {
            if (ioctl(fd, SIOCGIFADDR, &ifr) == 0) {
                // Not sure at all that this branch is ever used on modern
                // systems. So just procrastinate until/if we ever need this.
#error please fill in the code to retrieve the IP address, 
            }
        }

        ret = 0;
        break;
    }
    if_freenameindex(ifaces);
    close(fd);
#endif /* GETIFADDRS */

#else /* WIN32 -> */

    DWORD dwBufLen = sizeof(IP_ADAPTER_INFO);
    PIP_ADAPTER_INFO pAdapterInfo = (IP_ADAPTER_INFO *)malloc(dwBufLen);
    if (pAdapterInfo == NULL) {
        fprintf(stderr, "getsyshwaddr: malloc failed\n");
        return -1;
    }

    // Make an initial call to GetAdaptersInfo to get the necessary
    // size into the dwBufLen variable
    if (GetAdaptersInfo(pAdapterInfo, &dwBufLen) == ERROR_BUFFER_OVERFLOW) {
        pAdapterInfo = (IP_ADAPTER_INFO *)realloc(pAdapterInfo, dwBufLen);
        if (pAdapterInfo == NULL) {
            fprintf(stderr, "getsyshwaddr: GetAdaptersInfo (call 1) failed\n");
            return -1;
        }
    }

    // Get the full list and walk it
    if (GetAdaptersInfo(pAdapterInfo, &dwBufLen) == NO_ERROR) {
		PIP_ADAPTER_INFO pAdapter = pAdapterInfo;
        for (; pAdapter != NULL; pAdapter = pAdapter->Next) {
			if (pAdapter == NULL)
				break;
//            fprintf(stderr, "Adapter name [%s] Description [%s] "
//					"input [%s]\n", pAdapter->AdapterName,
//					pAdapter->Description,	iface);

            /* CurrentIpAddress->IpAddress.String stores the IPv4 address 
               as a char string in dotted notation */
            /* Skip localhost ? */
			const char *firstipstr = pAdapter->IpAddressList.IpAddress.String;
#if 0
			IP_ADDR_STRING *nextip = &pAdapter->IpAddressList;
			while (nextip != 0) {
				fprintf(stderr, "IP: [%s]\n", nextip->IpAddress.String);
				nextip = nextip->Next;
			}
#endif
            if (!strncmp("127.0.0", firstipstr, 7)) {
                continue;
            }

            // Note: AdapterName is a GUID which as far as I can see
            // appears nowhere in the GUI. Description is something
            // like "Intel Pro 100 #2", better.

            if (ifreporter) {
                ifreporter(tok, pAdapter->Description);
            }

            /* If the interface name was specified, check it */
            if (ifnamelen && strcmp(iface, pAdapter->Description))
                continue;
            
            /* Store the IP address in dotted notation format */
            if (ip) {
                strncpy(ip, firstipstr, ilen);
                ip[ilen-1] = 0;
            }

            /* The MAC is in the pAdapter->Address char array */
            memcpy(mac, pAdapter->Address, 6);

            if (!MACADDR_IS_ZERO(mac)) {
                ret = 0;
                break;
            }
        }
    } else {
        fprintf(stderr, "getsyshwaddr: GetAdaptersInfo (call 2) failed\n");
    }
    if (pAdapterInfo)
        free(pAdapterInfo);

#endif

    if (ret == 0 && buf) {
        if (hlen > 12)
            sprintf(buf, "%02x%02x%02x%02x%02x%02x",
                    mac[0]&0xFF, mac[1]&0xFF, mac[2]&0xFF,
                    mac[3]&0xFF, mac[4]&0xFF, mac[5]&0xFF);
        else if (hlen == 6)
            memmove(buf, mac, 6);
    }
    return ret;
}

#ifdef __cplusplus
}
#endif

/* Local Variables: */
/* mode: c++ */
/* c-basic-offset: 4 */
/* tab-width: 4 */
/* indent-tabs-mode: t */
/* End: */
