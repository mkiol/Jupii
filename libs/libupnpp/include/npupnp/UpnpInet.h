#ifndef UPNPINET_H
#define UPNPINET_H

/* Provide a platform independent way to include TCP/IP types and functions. */

#ifdef _WIN32

#include <stdarg.h>
#include <winsock2.h>
#include <iphlpapi.h>
#include <ws2tcpip.h>

#if(_WIN32_WINNT < 0x0600)
typedef short sa_family_t;
#else
typedef ADDRESS_FAMILY sa_family_t;
#endif

#define UpnpCloseSocket(s)  do {closesocket(s); s = INVALID_SOCKET;} while(0)
#define UPNP_SOCK_GET_LAST_ERROR() WSAGetLastError()

#else /* ! _WIN32 -> */

#include <unistd.h>

/*** Windows compatibility macros */
#define UpnpCloseSocket(s) do {close(s); s = -1;} while(0)
#define UPNP_SOCK_GET_LAST_ERROR() errno
/* SOCKET is typedefd by the system and unsigned on Win32 */
typedef int SOCKET;
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)
/*** End Windows compat */

#if defined(__sun)
#  include <fcntl.h>
#  include <sys/sockio.h>
#endif

#include <sys/types.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>


#endif /* ! _WIN32 */

#endif /* UPNPINET_H */
