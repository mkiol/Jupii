CONFIG += c++1z json no_lflags_merge
QT += gui network dbus sql multimedia xml
QMAKE_CXXFLAGS += -Wpedantic

CORE_DIR = ../core

include($$PROJECTDIR/libs/qhttpserver/qhttpserver.pri)

message(Qt is installed in $$[QT_INSTALL_PREFIX])

QT_BIN_DIR = "$$[QT_INSTALL_PREFIX]/bin"

exists("$$QT_BIN_DIR/qdbusxml2cpp-qt5") {
    QDBUSXML2CPP = "$$QT_BIN_DIR/qdbusxml2cpp-qt5"
} else {
    QDBUSXML2CPP = "$$QT_BIN_DIR/qdbusxml2cpp"
}
system("$$QDBUSXML2CPP" "$$PROJECTDIR/dbus/jupii.xml" -a $$CORE_DIR/dbus_jupii_adaptor)
system("$$QDBUSXML2CPP" "$$PROJECTDIR/dbus/tracker.xml" -p $$CORE_DIR/dbus_tracker_inf)
system("$$QDBUSXML2CPP" "$$PROJECTDIR/dbus/notifications.xml" -p $$CORE_DIR/dbus_notifications_inf)

OTHER_FILES += \
    $$PROJECTDIR/dbus/*.xml

desktop {
    PKGCONFIG += libpulse x11

    packagesExist(taglib) {
        PKGCONFIG += taglib
    } else {
        message(Using static taglib)
        include($$PROJECTDIR/libs/taglib/taglib.pri)
    }

    packagesExist(libupnpp libnpupnp libmicrohttpd expat libcurl) {
        PKGCONFIG += libnpupnp libmicrohttpd expat libcurl
        LIBS += -lupnpp -lupnpputil
    } else {
        message(Using static libupnpp libnpupnp libmicrohttpd expat libcurl)
        include($$PROJECTDIR/libs/libupnpp/libupnpp.pri)
    }

    packagesExist(gumbo) {
        PKGCONFIG += gumbo
    } else {
        message(Using static gumbo)
        include($$PROJECTDIR/libs/gumbo/gumbo.pri)
    }

    INCLUDEPATH += $$PROJECTDIR/libs/ffmpeg/include

    packagesExist(libavformat libavcodec libswscale libswresample libavdevice libavutil x264 lame) {
        PKGCONFIG += x264 lame libavformat libavcodec libswscale \
                     libswresample libavdevice libavutil
    } else {
        message(Using static ffmpeg x264 lame)
        include($$PROJECTDIR/libs/ffmpeg/ffmpeg.pri)
        include($$PROJECTDIR/libs/x264/x264.pri)
        include($$PROJECTDIR/libs/lame/lame.pri)
    }
}

sailfish {
    PKGCONFIG += libpulse keepalive

    include($$PROJECTDIR/libs/libupnpp/libupnpp.pri)
    include($$PROJECTDIR/libs/taglib/taglib.pri)
    include($$PROJECTDIR/libs/ffmpeg/ffmpeg.pri)
    include($$PROJECTDIR/libs/x264/x264.pri)
    include($$PROJECTDIR/libs/lame/lame.pri)
    include($$PROJECTDIR/libs/gumbo/gumbo.pri)
    #include($$PROJECTDIR/libs/omx/omx.pri)
    include($$PROJECTDIR/libs/lipstickrecorder/lipstickrecorder.pri)
}

INCLUDEPATH += $$CORE_DIR

HEADERS += \
    $$CORE_DIR/dbus_jupii_adaptor.h \
    $$CORE_DIR/dbus_tracker_inf.h \
    $$CORE_DIR/dbus_notifications_inf.h \
    $$CORE_DIR/utils.h \
    $$CORE_DIR/listmodel.h \
    $$CORE_DIR/devicemodel.h \
    $$CORE_DIR/renderingcontrol.h \
    $$CORE_DIR/avtransport.h \
    $$CORE_DIR/service.h \
    $$CORE_DIR/contentserver.h \
    $$CORE_DIR/filemetadata.h \
    $$CORE_DIR/settings.h \
    $$CORE_DIR/directory.h \
    $$CORE_DIR/taskexecutor.h \
    $$CORE_DIR/deviceinfo.h \
    $$CORE_DIR/playlistmodel.h \
    $$CORE_DIR/dbusapp.h \
    $$CORE_DIR/tracker.h \
    $$CORE_DIR/trackercursor.h \
    $$CORE_DIR/albummodel.h \
    $$CORE_DIR/artistmodel.h \
    $$CORE_DIR/playlistfilemodel.h \
    $$CORE_DIR/trackmodel.h \
    $$CORE_DIR/services.h \
    $$CORE_DIR/info.h \
    $$CORE_DIR/somafmmodel.h \
    $$CORE_DIR/fosdemmodel.h \
    $$CORE_DIR/gpoddermodel.h \
    $$CORE_DIR/itemmodel.h \
    $$CORE_DIR/icecastmodel.h \
    $$CORE_DIR/yamahaxc.h \
    $$CORE_DIR/dirmodel.h \
    $$CORE_DIR/recmodel.h \
    $$CORE_DIR/log.h \
    $$CORE_DIR/audiocaster.h \
    $$CORE_DIR/miccaster.h \
    $$CORE_DIR/pulseaudiosource.h \
    $$CORE_DIR/device.h \
    $$CORE_DIR/iconprovider.h \
    $$CORE_DIR/contentdirectory.h \
    $$CORE_DIR/cdirmodel.h \
    $$CORE_DIR/ytdlapi.h \
    $$CORE_DIR/bcmodel.h \
    $$CORE_DIR/notifications.h \
    $$CORE_DIR/filedownloader.h \
    $$CORE_DIR/bcapi.h \
    $$CORE_DIR/tuneinmodel.h \
    $$CORE_DIR/xc.h \
    $$CORE_DIR/frontiersiliconxc.h \
    $$CORE_DIR/connectivitydetector.h \
    $$CORE_DIR/soundcloudmodel.h \
    $$CORE_DIR/gumbotools.h \
    $$CORE_DIR/soundcloudapi.h \
    $$CORE_DIR/downloader.h \
    $$CORE_DIR/playlistparser.h \
    $$CORE_DIR/contentserverworker.h \
    $$CORE_DIR/screencaster.h \
    $$CORE_DIR/dnscontentdeterminator.h

SOURCES += \
    $$CORE_DIR/dbus_jupii_adaptor.cpp \
    $$CORE_DIR/dbus_tracker_inf.cpp \
    $$CORE_DIR/dbus_notifications_inf.cpp \
    $$CORE_DIR/main.cpp \
    $$CORE_DIR/utils.cpp \
    $$CORE_DIR/listmodel.cpp \
    $$CORE_DIR/devicemodel.cpp \
    $$CORE_DIR/renderingcontrol.cpp \
    $$CORE_DIR/avtransport.cpp \
    $$CORE_DIR/service.cpp \
    $$CORE_DIR/contentserver.cpp \
    $$CORE_DIR/filemetadata.cpp \
    $$CORE_DIR/settings.cpp \
    $$CORE_DIR/directory.cpp \
    $$CORE_DIR/taskexecutor.cpp \
    $$CORE_DIR/deviceinfo.cpp \
    $$CORE_DIR/playlistmodel.cpp \
    $$CORE_DIR/dbusapp.cpp \
    $$CORE_DIR/tracker.cpp \
    $$CORE_DIR/trackercursor.cpp \
    $$CORE_DIR/albummodel.cpp \
    $$CORE_DIR/artistmodel.cpp \
    $$CORE_DIR/playlistfilemodel.cpp \
    $$CORE_DIR/trackmodel.cpp \
    $$CORE_DIR/services.cpp \
    $$CORE_DIR/somafmmodel.cpp \
    $$CORE_DIR/fosdemmodel.cpp \
    $$CORE_DIR/gpoddermodel.cpp \
    $$CORE_DIR/itemmodel.cpp \
    $$CORE_DIR/icecastmodel.cpp \
    $$CORE_DIR/yamahaxc.cpp \
    $$CORE_DIR/dirmodel.cpp \
    $$CORE_DIR/recmodel.cpp \
    $$CORE_DIR/log.cpp \
    $$CORE_DIR/audiocaster.cpp \
    $$CORE_DIR/miccaster.cpp \
    $$CORE_DIR/pulseaudiosource.cpp \
    $$CORE_DIR/device.cpp \
    $$CORE_DIR/iconprovider.cpp \
    $$CORE_DIR/contentdirectory.cpp \
    $$CORE_DIR/cdirmodel.cpp \
    $$CORE_DIR/ytdlapi.cpp \
    $$CORE_DIR/bcmodel.cpp \
    $$CORE_DIR/notifications.cpp \
    $$CORE_DIR/filedownloader.cpp \
    $$CORE_DIR/bcapi.cpp \
    $$CORE_DIR/tuneinmodel.cpp \
    $$CORE_DIR/xc.cpp \
    $$CORE_DIR/frontiersiliconxc.cpp \
    $$CORE_DIR/connectivitydetector.cpp \
    $$CORE_DIR/soundcloudmodel.cpp \
    $$CORE_DIR/gumbotools.cpp \
    $$CORE_DIR/soundcloudapi.cpp \
    $$CORE_DIR/downloader.cpp \
    $$CORE_DIR/playlistparser.cpp \
    $$CORE_DIR/contentserverworker.cpp \
    $$CORE_DIR/screencaster.cpp \
    $$CORE_DIR/dnscontentdeterminator.cpp

DISTFILES += \
    $$PWD/../dbus/org.freedesktop.Tracker3.Endpoint.xml
