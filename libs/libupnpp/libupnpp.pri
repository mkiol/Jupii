LIBUPNPP_ROOT = $$PROJECTDIR/libs/libupnpp

PKGCONFIG += libcurl expat
LIBS += -lpthread

QMAKE_CXXFLAGS += -std=c++11 -D_FILE_OFFSET_BITS=64 -pthread

QMAKE_LFLAGS += -Wl,-zdefs

libupnpp_HEADERS += \
  $$LIBUPNPP_ROOT/libupnpp/control/avlastchg.hxx \
  $$LIBUPNPP_ROOT/libupnpp/control/avtransport.hxx \
  $$LIBUPNPP_ROOT/libupnpp/control/cdircontent.hxx \
  $$LIBUPNPP_ROOT/libupnpp/control/cdirectory.hxx \
  $$LIBUPNPP_ROOT/libupnpp/control/description.hxx \
  $$LIBUPNPP_ROOT/libupnpp/control/device.hxx \
  $$LIBUPNPP_ROOT/libupnpp/control/discovery.hxx \
  $$LIBUPNPP_ROOT/libupnpp/control/httpdownload.hxx \
  $$LIBUPNPP_ROOT/libupnpp/control/linnsongcast.hxx \
  $$LIBUPNPP_ROOT/libupnpp/control/mediarenderer.hxx \
  $$LIBUPNPP_ROOT/libupnpp/control/mediaserver.hxx \
  $$LIBUPNPP_ROOT/libupnpp/control/ohplaylist.hxx \
  $$LIBUPNPP_ROOT/libupnpp/control/ohproduct.hxx \
  $$LIBUPNPP_ROOT/libupnpp/control/ohradio.hxx \
  $$LIBUPNPP_ROOT/libupnpp/control/ohinfo.hxx \
  $$LIBUPNPP_ROOT/libupnpp/control/ohreceiver.hxx \
  $$LIBUPNPP_ROOT/libupnpp/control/ohsender.hxx \
  $$LIBUPNPP_ROOT/libupnpp/control/ohtime.hxx \
  $$LIBUPNPP_ROOT/libupnpp/control/ohvolume.hxx \
  $$LIBUPNPP_ROOT/libupnpp/control/renderingcontrol.hxx \
  $$LIBUPNPP_ROOT/libupnpp/control/service.hxx \
  $$LIBUPNPP_ROOT/libupnpp/device/device.hxx \
  $$LIBUPNPP_ROOT/libupnpp/device/vdir.hxx \
  $$LIBUPNPP_ROOT/libupnpp/base64.hxx \
  $$LIBUPNPP_ROOT/libupnpp/expatmm.hxx \
  $$LIBUPNPP_ROOT/libupnpp/getsyshwaddr.h \
  $$LIBUPNPP_ROOT/libupnpp/ixmlwrap.hxx \
  $$LIBUPNPP_ROOT/libupnpp/log.h \
  $$LIBUPNPP_ROOT/libupnpp/log.hxx \
  $$LIBUPNPP_ROOT/libupnpp/md5.hxx \
  $$LIBUPNPP_ROOT/libupnpp/smallut.h \
  $$LIBUPNPP_ROOT/libupnpp/soaphelp.hxx \
  $$LIBUPNPP_ROOT/libupnpp/upnpavutils.hxx \
  $$LIBUPNPP_ROOT/libupnpp/upnpp_p.hxx \
  $$LIBUPNPP_ROOT/libupnpp/upnpplib.hxx \
  $$LIBUPNPP_ROOT/libupnpp/workqueue.h \
  $$LIBUPNPP_ROOT/libupnpp/conf_post.h \
  $$LIBUPNPP_ROOT/libupnpp/config.h

SOURCES += \
  $$LIBUPNPP_ROOT/libupnpp/control/avlastchg.cxx \
  $$LIBUPNPP_ROOT/libupnpp/control/avtransport.cxx \
  $$LIBUPNPP_ROOT/libupnpp/control/cdircontent.cxx \
  $$LIBUPNPP_ROOT/libupnpp/control/cdirectory.cxx \
  $$LIBUPNPP_ROOT/libupnpp/control/description.cxx \
  $$LIBUPNPP_ROOT/libupnpp/control/device.cxx \
  $$LIBUPNPP_ROOT/libupnpp/control/discovery.cxx \
  $$LIBUPNPP_ROOT/libupnpp/control/httpdownload.cxx \
  $$LIBUPNPP_ROOT/libupnpp/control/linnsongcast.cxx \
  $$LIBUPNPP_ROOT/libupnpp/control/mediarenderer.cxx \
  $$LIBUPNPP_ROOT/libupnpp/control/mediaserver.cxx \
  $$LIBUPNPP_ROOT/libupnpp/control/ohplaylist.cxx \
  $$LIBUPNPP_ROOT/libupnpp/control/ohproduct.cxx \
  $$LIBUPNPP_ROOT/libupnpp/control/ohradio.cxx \
  $$LIBUPNPP_ROOT/libupnpp/control/ohinfo.cxx \
  $$LIBUPNPP_ROOT/libupnpp/control/ohreceiver.cxx \
  $$LIBUPNPP_ROOT/libupnpp/control/ohsender.cxx \
  $$LIBUPNPP_ROOT/libupnpp/control/ohtime.cxx \
  $$LIBUPNPP_ROOT/libupnpp/control/ohvolume.cxx \
  $$LIBUPNPP_ROOT/libupnpp/control/renderingcontrol.cxx \
  $$LIBUPNPP_ROOT/libupnpp/control/service.cxx \
  $$LIBUPNPP_ROOT/libupnpp/device/device.cxx \
  $$LIBUPNPP_ROOT/libupnpp/device/vdir.cxx \
  $$LIBUPNPP_ROOT/libupnpp/base64.cxx \
  $$LIBUPNPP_ROOT/libupnpp/getsyshwaddr.cpp \
  $$LIBUPNPP_ROOT/libupnpp/ixmlwrap.cxx \
  $$LIBUPNPP_ROOT/libupnpp/log.cpp \
  $$LIBUPNPP_ROOT/libupnpp/md5.cxx \
  $$LIBUPNPP_ROOT/libupnpp/smallut.cpp \
  $$LIBUPNPP_ROOT/libupnpp/soaphelp.cxx \
  $$LIBUPNPP_ROOT/libupnpp/upnpavutils.cxx \
  $$LIBUPNPP_ROOT/libupnpp/upnpplib.cxx

INCLUDEPATH += \
  $$LIBUPNPP_ROOT \
  $$LIBUPNPP_ROOT/libupnpp
