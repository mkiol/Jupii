CONFIG += c++1z json no_lflags_merge
QT += gui network dbus sql multimedia xml
QMAKE_CXXFLAGS += -Wpedantic

ROOT_DIR = ../src

include($$PROJECTDIR/libs/qhttpserver/qhttpserver.pri)

message(Qt is installed in $$[QT_INSTALL_PREFIX])

QT_BIN_DIR = "$$[QT_INSTALL_PREFIX]/bin"

exists("$$QT_BIN_DIR/qdbusxml2cpp-qt5") {
    QDBUSXML2CPP = "$$QT_BIN_DIR/qdbusxml2cpp-qt5"
} else {
    QDBUSXML2CPP = "$$QT_BIN_DIR/qdbusxml2cpp"
}
system("$$QDBUSXML2CPP" "$$PROJECTDIR/dbus/jupii.xml" -a $$ROOT_DIR/dbus_jupii_adaptor)
system("$$QDBUSXML2CPP" "$$PROJECTDIR/dbus/tracker.xml" -p $$ROOT_DIR/dbus_tracker_inf)
system("$$QDBUSXML2CPP" "$$PROJECTDIR/dbus/notifications.xml" -p $$ROOT_DIR/dbus_notifications_inf)

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

#    packagesExist(libavformat libavcodec libswscale libswresample libavdevice libavutil x264 lame) {
#        PKGCONFIG += x264 lame libavformat libavcodec libswscale \
#                     libswresample libavdevice libavutil
#    } else {
        message(Using static ffmpeg x264 lame)
        include($$PROJECTDIR/libs/ffmpeg/ffmpeg.pri)
        include($$PROJECTDIR/libs/x264/x264.pri)
        include($$PROJECTDIR/libs/lame/lame.pri)
#    }
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

INCLUDEPATH += $$ROOT_DIR

HEADERS += \
    $$ROOT_DIR/dbus_jupii_adaptor.h \
    $$ROOT_DIR/dbus_tracker_inf.h \
    $$ROOT_DIR/dbus_notifications_inf.h \
    $$ROOT_DIR/utils.h \
    $$ROOT_DIR/listmodel.h \
    $$ROOT_DIR/devicemodel.h \
    $$ROOT_DIR/renderingcontrol.h \
    $$ROOT_DIR/avtransport.h \
    $$ROOT_DIR/service.h \
    $$ROOT_DIR/contentserver.h \
    $$ROOT_DIR/filemetadata.h \
    $$ROOT_DIR/settings.h \
    $$ROOT_DIR/directory.h \
    $$ROOT_DIR/taskexecutor.h \
    $$ROOT_DIR/deviceinfo.h \
    $$ROOT_DIR/playlistmodel.h \
    $$ROOT_DIR/dbusapp.h \
    $$ROOT_DIR/tracker.h \
    $$ROOT_DIR/trackercursor.h \
    $$ROOT_DIR/albummodel.h \
    $$ROOT_DIR/artistmodel.h \
    $$ROOT_DIR/playlistfilemodel.h \
    $$ROOT_DIR/trackmodel.h \
    $$ROOT_DIR/services.h \
    $$ROOT_DIR/info.h \
    $$ROOT_DIR/somafmmodel.h \
    $$ROOT_DIR/fosdemmodel.h \
    $$ROOT_DIR/gpoddermodel.h \
    $$ROOT_DIR/itemmodel.h \
    $$ROOT_DIR/icecastmodel.h \
    $$ROOT_DIR/yamahaxc.h \
    $$ROOT_DIR/dirmodel.h \
    $$ROOT_DIR/recmodel.h \
    $$ROOT_DIR/log.h \
    $$ROOT_DIR/audiocaster.h \
    $$ROOT_DIR/miccaster.h \
    $$ROOT_DIR/pulseaudiosource.h \
    $$ROOT_DIR/device.h \
    $$ROOT_DIR/iconprovider.h \
    $$ROOT_DIR/contentdirectory.h \
    $$ROOT_DIR/cdirmodel.h \
    $$ROOT_DIR/ytdlapi.h \
    $$ROOT_DIR/bcmodel.h \
    $$ROOT_DIR/notifications.h \
    $$ROOT_DIR/filedownloader.h \
    $$ROOT_DIR/bcapi.h \
    $$ROOT_DIR/tuneinmodel.h \
    $$ROOT_DIR/xc.h \
    $$ROOT_DIR/frontiersiliconxc.h \
    $$ROOT_DIR/connectivitydetector.h \
    $$ROOT_DIR/soundcloudmodel.h \
    $$ROOT_DIR/gumbotools.h \
    $$ROOT_DIR/soundcloudapi.h \
    $$ROOT_DIR/downloader.h \
    $$ROOT_DIR/playlistparser.h \
    $$ROOT_DIR/contentserverworker.h \
    $$ROOT_DIR/screencaster.h \
    $$ROOT_DIR/dnscontentdeterminator.h

SOURCES += \
    $$ROOT_DIR/dbus_jupii_adaptor.cpp \
    $$ROOT_DIR/dbus_tracker_inf.cpp \
    $$ROOT_DIR/dbus_notifications_inf.cpp \
    $$ROOT_DIR/main.cpp \
    $$ROOT_DIR/utils.cpp \
    $$ROOT_DIR/listmodel.cpp \
    $$ROOT_DIR/devicemodel.cpp \
    $$ROOT_DIR/renderingcontrol.cpp \
    $$ROOT_DIR/avtransport.cpp \
    $$ROOT_DIR/service.cpp \
    $$ROOT_DIR/contentserver.cpp \
    $$ROOT_DIR/filemetadata.cpp \
    $$ROOT_DIR/settings.cpp \
    $$ROOT_DIR/directory.cpp \
    $$ROOT_DIR/taskexecutor.cpp \
    $$ROOT_DIR/deviceinfo.cpp \
    $$ROOT_DIR/playlistmodel.cpp \
    $$ROOT_DIR/dbusapp.cpp \
    $$ROOT_DIR/tracker.cpp \
    $$ROOT_DIR/trackercursor.cpp \
    $$ROOT_DIR/albummodel.cpp \
    $$ROOT_DIR/artistmodel.cpp \
    $$ROOT_DIR/playlistfilemodel.cpp \
    $$ROOT_DIR/trackmodel.cpp \
    $$ROOT_DIR/services.cpp \
    $$ROOT_DIR/somafmmodel.cpp \
    $$ROOT_DIR/fosdemmodel.cpp \
    $$ROOT_DIR/gpoddermodel.cpp \
    $$ROOT_DIR/itemmodel.cpp \
    $$ROOT_DIR/icecastmodel.cpp \
    $$ROOT_DIR/yamahaxc.cpp \
    $$ROOT_DIR/dirmodel.cpp \
    $$ROOT_DIR/recmodel.cpp \
    $$ROOT_DIR/log.cpp \
    $$ROOT_DIR/audiocaster.cpp \
    $$ROOT_DIR/miccaster.cpp \
    $$ROOT_DIR/pulseaudiosource.cpp \
    $$ROOT_DIR/device.cpp \
    $$ROOT_DIR/iconprovider.cpp \
    $$ROOT_DIR/contentdirectory.cpp \
    $$ROOT_DIR/cdirmodel.cpp \
    $$ROOT_DIR/ytdlapi.cpp \
    $$ROOT_DIR/bcmodel.cpp \
    $$ROOT_DIR/notifications.cpp \
    $$ROOT_DIR/filedownloader.cpp \
    $$ROOT_DIR/bcapi.cpp \
    $$ROOT_DIR/tuneinmodel.cpp \
    $$ROOT_DIR/xc.cpp \
    $$ROOT_DIR/frontiersiliconxc.cpp \
    $$ROOT_DIR/connectivitydetector.cpp \
    $$ROOT_DIR/soundcloudmodel.cpp \
    $$ROOT_DIR/gumbotools.cpp \
    $$ROOT_DIR/soundcloudapi.cpp \
    $$ROOT_DIR/downloader.cpp \
    $$ROOT_DIR/playlistparser.cpp \
    $$ROOT_DIR/contentserverworker.cpp \
    $$ROOT_DIR/screencaster.cpp \
    $$ROOT_DIR/dnscontentdeterminator.cpp

DISTFILES += \
    $$PWD/../dbus/org.freedesktop.Tracker3.Endpoint.xml
