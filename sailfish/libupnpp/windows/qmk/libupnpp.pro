QT       -= core gui

TARGET = upnpp
TEMPLATE = lib

# Need this because we have 2 device.cxx files
CONFIG += object_parallel_to_source
CONFIG  += qt warn_on thread release
CONFIG += staticlib

DEFINES += UPNP_STATIC_LIB
DEFINES += CURL_STATICLIB
DEFINES += WIN32
DEFINES -= UNICODE
DEFINES -= _UNICODE
DEFINES += _MBCS
DEFINES += PSAPI_VERSION=1

INCLUDEPATH += ../../
INCLUDEPATH += c:/users/bill/documents/upnp/expat-2.1.0/lib
INCLUDEPATH += c:/users/bill/documents/upnp/pupnp/include
INCLUDEPATH += c:/users/bill/documents/upnp/curl-7.43.0/include

LIBS += c:/users/bill/documents/upnp/expat-2.1.0/.libs/libexpat.a
LIBS += c:/users/bill/documents/upnp/curl-7.43.0/lib/libcurl.a
LIBS += c:/users/bill/documents/upnp/pupnp/upnp/.libs/libupnp.a
LIBS += c:/users/bill/documents/upnp/pupnp/ixml/.libs/libixml.a
LIBS += c:/users/bill/documents/upnp/pupnp/threadutil/.libs/libthreadutil.a
LIBS += -liphlpapi
LIBS += -lwldap32
LIBS += -lws2_32

contains(QMAKE_CC, gcc){
    # MingW
    QMAKE_CXXFLAGS += -std=c++11 -Wno-unused-parameter
}
contains(QMAKE_CC, cl){
    # Visual Studio
}

SOURCES += \
../../libupnpp/base64.cxx \
../../libupnpp/control/avlastchg.cxx \
../../libupnpp/control/avtransport.cxx \
../../libupnpp/control/cdircontent.cxx \
../../libupnpp/control/cdirectory.cxx \
../../libupnpp/control/description.cxx \
../../libupnpp/control/device.cxx \
../../libupnpp/control/discovery.cxx \
../../libupnpp/control/httpdownload.cxx \
../../libupnpp/control/linnsongcast.cxx \
../../libupnpp/control/mediarenderer.cxx \
../../libupnpp/control/mediaserver.cxx \
../../libupnpp/control/ohinfo.cxx \
../../libupnpp/control/ohplaylist.cxx \
../../libupnpp/control/ohproduct.cxx \
../../libupnpp/control/ohradio.cxx \
../../libupnpp/control/ohreceiver.cxx \
../../libupnpp/control/ohsender.cxx \
../../libupnpp/control/ohtime.cxx \
../../libupnpp/control/ohvolume.cxx \
../../libupnpp/control/renderingcontrol.cxx \
../../libupnpp/control/service.cxx \
../../libupnpp/device/device.cxx \
../../libupnpp/device/vdir.cxx \
../../libupnpp/getsyshwaddr.c \
../../libupnpp/ixmlwrap.cxx \
../../libupnpp/log.cpp \
../../libupnpp/md5.cxx \
../../libupnpp/smallut.cpp \
../../libupnpp/soaphelp.cxx \
../../libupnpp/upnpavutils.cxx \
../../libupnpp/upnpplib.cxx
