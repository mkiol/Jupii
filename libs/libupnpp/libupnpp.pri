LIBS += -lpthread -lcurl -lexpat

QMAKE_CXXFLAGS += -std=c++11 -D_FILE_OFFSET_BITS=64 -pthread

QMAKE_LFLAGS += -Wl,-zdefs

libupnpp_BASE = ../libs/libupnpp
pupnp_BASE = ../libs/pupnp

libupnpp_HEADERS += \
  $$libupnpp_BASE/libupnpp/control/avlastchg.hxx \
  $$libupnpp_BASE/libupnpp/control/avtransport.hxx \
  $$libupnpp_BASE/libupnpp/control/cdircontent.hxx \
  $$libupnpp_BASE/libupnpp/control/cdirectory.hxx \
  $$libupnpp_BASE/libupnpp/control/description.hxx \
  $$libupnpp_BASE/libupnpp/control/device.hxx \
  $$libupnpp_BASE/libupnpp/control/discovery.hxx \
  $$libupnpp_BASE/libupnpp/control/httpdownload.hxx \
  $$libupnpp_BASE/libupnpp/control/linnsongcast.hxx \
  $$libupnpp_BASE/libupnpp/control/mediarenderer.hxx \
  $$libupnpp_BASE/libupnpp/control/mediaserver.hxx \
  $$libupnpp_BASE/libupnpp/control/ohplaylist.hxx \
  $$libupnpp_BASE/libupnpp/control/ohproduct.hxx \
  $$libupnpp_BASE/libupnpp/control/ohradio.hxx \
  $$libupnpp_BASE/libupnpp/control/ohinfo.hxx \
  $$libupnpp_BASE/libupnpp/control/ohreceiver.hxx \
  $$libupnpp_BASE/libupnpp/control/ohsender.hxx \
  $$libupnpp_BASE/libupnpp/control/ohtime.hxx \
  $$libupnpp_BASE/libupnpp/control/ohvolume.hxx \
  $$libupnpp_BASE/libupnpp/control/renderingcontrol.hxx \
  $$libupnpp_BASE/libupnpp/control/service.hxx \
  $$libupnpp_BASE/libupnpp/device/device.hxx \
  $$libupnpp_BASE/libupnpp/device/vdir.hxx \
  $$libupnpp_BASE/libupnpp/base64.hxx \
  $$libupnpp_BASE/libupnpp/expatmm.hxx \
  $$libupnpp_BASE/libupnpp/getsyshwaddr.h \
  $$libupnpp_BASE/libupnpp/ixmlwrap.hxx \
  $$libupnpp_BASE/libupnpp/log.h \
  $$libupnpp_BASE/libupnpp/log.hxx \
  $$libupnpp_BASE/libupnpp/md5.hxx \
  $$libupnpp_BASE/libupnpp/smallut.h \
  $$libupnpp_BASE/libupnpp/soaphelp.hxx \
  $$libupnpp_BASE/libupnpp/upnpavutils.hxx \
  $$libupnpp_BASE/libupnpp/upnpp_p.hxx \
  $$libupnpp_BASE/libupnpp/upnpplib.hxx \
  $$libupnpp_BASE/libupnpp/workqueue.h \
  $$libupnpp_BASE/libupnpp/conf_post.h \
  $$libupnpp_BASE/libupnpp/config.h

#HEADERS += $$libupnpp_HEADERS

SOURCES += \
  $$libupnpp_BASE/libupnpp/control/avlastchg.cxx \
  $$libupnpp_BASE/libupnpp/control/avtransport.cxx \
  $$libupnpp_BASE/libupnpp/control/cdircontent.cxx \
  $$libupnpp_BASE/libupnpp/control/cdirectory.cxx \
  $$libupnpp_BASE/libupnpp/control/description.cxx \
  $$libupnpp_BASE/libupnpp/control/device.cxx \
  $$libupnpp_BASE/libupnpp/control/discovery.cxx \
  $$libupnpp_BASE/libupnpp/control/httpdownload.cxx \
  $$libupnpp_BASE/libupnpp/control/linnsongcast.cxx \
  $$libupnpp_BASE/libupnpp/control/mediarenderer.cxx \
  $$libupnpp_BASE/libupnpp/control/mediaserver.cxx \
  $$libupnpp_BASE/libupnpp/control/ohplaylist.cxx \
  $$libupnpp_BASE/libupnpp/control/ohproduct.cxx \
  $$libupnpp_BASE/libupnpp/control/ohradio.cxx \
  $$libupnpp_BASE/libupnpp/control/ohinfo.cxx \
  $$libupnpp_BASE/libupnpp/control/ohreceiver.cxx \
  $$libupnpp_BASE/libupnpp/control/ohsender.cxx \
  $$libupnpp_BASE/libupnpp/control/ohtime.cxx \
  $$libupnpp_BASE/libupnpp/control/ohvolume.cxx \
  $$libupnpp_BASE/libupnpp/control/renderingcontrol.cxx \
  $$libupnpp_BASE/libupnpp/control/service.cxx \
  $$libupnpp_BASE/libupnpp/device/device.cxx \
  $$libupnpp_BASE/libupnpp/device/vdir.cxx \
  $$libupnpp_BASE/libupnpp/base64.cxx \
  $$libupnpp_BASE/libupnpp/getsyshwaddr.cpp \
  $$libupnpp_BASE/libupnpp/ixmlwrap.cxx \
  $$libupnpp_BASE/libupnpp/log.cpp \
  $$libupnpp_BASE/libupnpp/md5.cxx \
  $$libupnpp_BASE/libupnpp/smallut.cpp \
  $$libupnpp_BASE/libupnpp/soaphelp.cxx \
  $$libupnpp_BASE/libupnpp/upnpavutils.cxx \
  $$libupnpp_BASE/libupnpp/upnpplib.cxx

INCLUDEPATH += \
  $$libupnpp_BASE \
  $$libupnpp_BASE/libupnpp

INCLUDEPATH += \
  $$pupnp_BASE \
  $$pupnp_BASE/upnp \
  $$pupnp_BASE/ixml \
  $$pupnp_BASE/threadutil

