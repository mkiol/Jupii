CORE_DIR = ../core

INCLUDEPATH += $$CORE_DIR

include($$PROJECTDIR/libs/qhttpserver/qhttpserver.pri)
include($$PROJECTDIR/libs/pupnp/pupnp.pri)
include($$PROJECTDIR/libs/libupnpp/libupnpp.pri)

message(Qt is installed in $$[QT_INSTALL_PREFIX])
QT_BIN_DIR = "$$[QT_INSTALL_PREFIX]/bin"
system("$$QT_BIN_DIR/qdbusxml2cpp" "$$PROJECTDIR/dbus/org.jupii.xml" -a $$CORE_DIR/dbus_jupii_adaptor)
system("$$QT_BIN_DIR/qdbusxml2cpp" "$$PROJECTDIR/dbus/org.freedesktop.Tracker1.Steroids.xml" -p $$CORE_DIR/dbus_tracker_inf)

desktop {
    LIBS += -ltag -lpulse
    INCLUDEPATH += /usr/include/taglib
    include($$PROJECTDIR/libs/ffmpeg/ffmpeg.pri)
    include($$PROJECTDIR/libs/x264/x264.pri)
}

sailfish {
    LIBS += -lpulse -lkeepalive
    include($$PROJECTDIR/libs/taglib/taglib.pri)
    include($$PROJECTDIR/libs/lame/lame.pri)
    include($$PROJECTDIR/libs/ffmpeg/ffmpeg.pri)
    include($$PROJECTDIR/libs/lame/lame.pri)
    include($$PROJECTDIR/libs/x264/x264.pri)
    screencast: include($$PROJECTDIR/libs/lipstickrecorder/lipstickrecorder.pri)
}

HEADERS += \
    $$CORE_DIR/dbus_jupii_adaptor.h \
    $$CORE_DIR/dbus_tracker_inf.h \
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
    $$CORE_DIR/yamahaextendedcontrol.h \
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
    $$CORE_DIR/youtubedl.h

SOURCES += \
    $$CORE_DIR/dbus_jupii_adaptor.cpp \
    $$CORE_DIR/dbus_tracker_inf.cpp \
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
    $$CORE_DIR/yamahaextendedcontrol.cpp \
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
    $$CORE_DIR/youtubedl.cpp

screencast {
    HEADERS += \
        $$CORE_DIR/screencaster.h

    SOURCES += \
        $$CORE_DIR/screencaster.cpp
}
