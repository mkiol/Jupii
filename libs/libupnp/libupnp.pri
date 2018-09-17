QMAKE_CFLAGS += -D_FILE_OFFSET_BITS=64 -pthread -g -O2 -Os -Wall

libupnp_BASE = ../libs/libupnp

libupnp_HEADERS = \
  $$libupnp_BASE/upnp/inc/UpnpString.h \
  $$libupnp_BASE/upnp/inc/upnp.h \
  $$libupnp_BASE/upnp/inc/upnpdebug.h \
  $$libupnp_BASE/upnp/inc/UpnpGlobal.h \
  $$libupnp_BASE/upnp/inc/UpnpInet.h \
  $$libupnp_BASE/upnp/inc/UpnpIntTypes.h \
  $$libupnp_BASE/upnp/inc/UpnpStdInt.h \
  $$libupnp_BASE/upnp/inc/UpnpUniStd.h \
  $$libupnp_BASE/upnp/src/inc/config.h \
  $$libupnp_BASE/upnp/src/inc/client_table.h \
  $$libupnp_BASE/upnp/src/inc/gena.h \
  $$libupnp_BASE/upnp/src/inc/gena_ctrlpt.h \
  $$libupnp_BASE/upnp/src/inc/gena_device.h \
  $$libupnp_BASE/upnp/src/inc/global.h \
  $$libupnp_BASE/upnp/src/inc/gmtdate.h \
  $$libupnp_BASE/upnp/src/inc/httpparser.h \
  $$libupnp_BASE/upnp/src/inc/httpreadwrite.h \
  $$libupnp_BASE/upnp/src/inc/md5.h \
  $$libupnp_BASE/upnp/src/inc/membuffer.h \
  $$libupnp_BASE/upnp/src/inc/miniserver.h \
  $$libupnp_BASE/upnp/src/inc/netall.h \
  $$libupnp_BASE/upnp/src/inc/parsetools.h \
  $$libupnp_BASE/upnp/src/inc/server.h \
  $$libupnp_BASE/upnp/src/inc/service_table.h \
  $$libupnp_BASE/upnp/src/inc/soaplib.h \
  $$libupnp_BASE/upnp/src/inc/sock.h \
  $$libupnp_BASE/upnp/src/inc/statcodes.h \
  $$libupnp_BASE/upnp/src/inc/statuscodes.h \
  $$libupnp_BASE/upnp/src/inc/strintmap.h \
  $$libupnp_BASE/upnp/src/inc/ssdplib.h \
  $$libupnp_BASE/upnp/src/inc/sysdep.h \
  $$libupnp_BASE/upnp/src/inc/unixutil.h \
  $$libupnp_BASE/upnp/src/inc/upnpapi.h \
  $$libupnp_BASE/upnp/src/inc/upnp_timeout.h \
  $$libupnp_BASE/upnp/src/inc/uri.h \
  $$libupnp_BASE/upnp/src/inc/urlconfig.h \
  $$libupnp_BASE/upnp/src/inc/upnputil.h \
  $$libupnp_BASE/upnp/src/inc/uuid.h \
  $$libupnp_BASE/upnp/src/inc/VirtualDir.h \
  $$libupnp_BASE/upnp/src/inc/webserver.h \
  $$libupnp_BASE/upnp/src/ssdp/ssdp_ResultData.h \
  $$libupnp_BASE/upnp/src/inc/inet_pton.h
  
libixml_HEADERS = \
  $$libupnp_BASE/ixml/src/inc/ixmlmembuf.h \
  $$libupnp_BASE/ixml/src/inc/ixmlparser.h \
  $$libupnp_BASE/ixml/inc/ixml.h \
  $$libupnp_BASE/ixml/inc/ixmldebug.h

libthreadutil_HEADERS = \
  $$libupnp_BASE/threadutil/inc/FreeList.h \
  $$libupnp_BASE/threadutil/inc/LinkedList.h \
  $$libupnp_BASE/threadutil/inc/ThreadPool.h \
  $$libupnp_BASE/threadutil/inc/TimerThread.h \
  $$libupnp_BASE/threadutil/inc/ithread.h

libupnp_SOURCES = \
  $$libupnp_BASE/upnp/src/ssdp/ssdp_device.c \
  $$libupnp_BASE/upnp/src/ssdp/ssdp_ctrlpt.c \
  $$libupnp_BASE/upnp/src/ssdp/ssdp_server.c \
  $$libupnp_BASE/upnp/src/soap/soap_device.c \
  $$libupnp_BASE/upnp/src/soap/soap_ctrlpt.c \
  $$libupnp_BASE/upnp/src/soap/soap_common.c \
  $$libupnp_BASE/upnp/src/genlib/miniserver/miniserver.c \
  $$libupnp_BASE/upnp/src/genlib/service_table/service_table.c \
  $$libupnp_BASE/upnp/src/genlib/util/membuffer.c \
  $$libupnp_BASE/upnp/src/genlib/util/strintmap.c \
  $$libupnp_BASE/upnp/src/genlib/util/upnp_timeout.c \
  $$libupnp_BASE/upnp/src/genlib/util/util.c \
  $$libupnp_BASE/upnp/src/genlib/client_table/client_table.c \
  $$libupnp_BASE/upnp/src/genlib/net/sock.c \
  $$libupnp_BASE/upnp/src/genlib/net/http/httpparser.c \
  $$libupnp_BASE/upnp/src/genlib/net/http/httpreadwrite.c \
  $$libupnp_BASE/upnp/src/genlib/net/http/statcodes.c \
  $$libupnp_BASE/upnp/src/genlib/net/http/webserver.c \
  $$libupnp_BASE/upnp/src/genlib/net/http/parsetools.c \
  $$libupnp_BASE/upnp/src/genlib/net/uri/uri.c \
  $$libupnp_BASE/upnp/src/gena/gena_device.c \
  $$libupnp_BASE/upnp/src/gena/gena_ctrlpt.c \
  $$libupnp_BASE/upnp/src/gena/gena_callback2.c \
  $$libupnp_BASE/upnp/src/api/UpnpString.c \
  $$libupnp_BASE/upnp/src/api/upnpapi.c \
  $$libupnp_BASE/upnp/src/api/upnptools.c \
  $$libupnp_BASE/upnp/src/api/upnpdebug.c \
  $$libupnp_BASE/upnp/src/uuid/md5.c \
  $$libupnp_BASE/upnp/src/uuid/sysdep.c \
  $$libupnp_BASE/upnp/src/uuid/uuid.c \
  $$libupnp_BASE/upnp/src/urlconfig/urlconfig.c \
  $$libupnp_BASE/upnp/src/inet_pton.c

libixml_SOURCES = \
  $$libupnp_BASE/ixml/src/attr.c \
  $$libupnp_BASE/ixml/src/document.c \
  $$libupnp_BASE/ixml/src/element.c \
  $$libupnp_BASE/ixml/src/ixml.c \
  $$libupnp_BASE/ixml/src/ixmldebug.c \
  $$libupnp_BASE/ixml/src/ixmlparser.c \
  $$libupnp_BASE/ixml/src/ixmlmembuf.c \
  $$libupnp_BASE/ixml/src/namedNodeMap.c \
  $$libupnp_BASE/ixml/src/node.c \
  $$libupnp_BASE/ixml/src/nodeList.c

libthreadutil_SOURCES = \
  $$libupnp_BASE/threadutil/src/FreeList.c \
  $$libupnp_BASE/threadutil/src/LinkedList.c \
  $$libupnp_BASE/threadutil/src/ThreadPool.c \
  $$libupnp_BASE/threadutil/src/TimerThread.c

HEADERS += \
  $$libupnp_HEADERS \
  $$libixml_HEADERS \
  $$libthreadutil_HEADERS

SOURCES += \
  $$libupnp_SOURCES \
  $$libixml_SOURCES \
  $$libthreadutil_SOURCES

INCLUDEPATH += \
  $$libupnp_BASE \
  $$libupnp_BASE/upnp \
  $$libupnp_BASE/build/inc \
  $$libupnp_BASE/upnp/src/inc \
  $$libupnp_BASE/threadutil/inc \
  $$libupnp_BASE/ixml/inc \
  $$libupnp_BASE/ixml/src/inc
