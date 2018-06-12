TARGET = harbour-jupii

CONFIG += c++11 sailfishapp object_parallel_to_source json no_lflags_merge
QT += dbus

PKGCONFIG += mlite5

INCLUDEPATH += /usr/include/c++/7

DEFINES += Q_OS_SAILFISH

include(qhttpserver/qhttpserver.pri)
include(libupnp/libupnp.pri)
include(libupnpp/libupnpp.pri)
include(taglib/taglib.pri)

linux-g++-32 {
    LIBS += -L$$PWD/libav/i486/ -l:libavutil.so.55 -l:libavformat.so.57 -l:libavcodec.so.57 -l:libavresample.so.3
}

linux-g++ {
    LIBS += -L$$PWD/libav/arm/ -l:libavutil.so.55 -l:libavformat.so.57 -l:libavcodec.so.57 -l:libavresample.so.3
}

INCLUDEPATH += $$PWD/libav/src

HEADERS += \
    src/dbus_jupii_adaptor.h \
    src/dbus_tracker_inf.h \
    src/utils.h \
    src/listmodel.h \
    src/devicemodel.h \
    src/renderingcontrol.h \
    src/avtransport.h \
    src/service.h \
    src/contentserver.h \
    src/filemetadata.h \
    src/settings.h \
    src/directory.h \
    src/taskexecutor.h \
    src/deviceinfo.h \
    src/iconprovider.h \
    src/playlistmodel.h \
    src/dbusapp.h \
    src/tracker.h \
    src/trackercursor.h \
    src/albummodel.h \
    src/artistmodel.h \
    src/playlistfilemodel.h \
    src/trackmodel.h \
    src/services.h

SOURCES += \
    src/dbus_jupii_adaptor.cpp \
    src/dbus_tracker_inf.cpp \
    src/main.cpp \
    src/utils.cpp \
    src/listmodel.cpp \
    src/devicemodel.cpp \
    src/renderingcontrol.cpp \
    src/avtransport.cpp \
    src/service.cpp \
    src/contentserver.cpp \
    src/filemetadata.cpp \
    src/settings.cpp \
    src/directory.cpp \
    src/taskexecutor.cpp \
    src/deviceinfo.cpp \
    src/iconprovider.cpp \
    src/playlistmodel.cpp \
    src/dbusapp.cpp \
    src/tracker.cpp \
    src/trackercursor.cpp \
    src/albummodel.cpp \
    src/artistmodel.cpp \
    src/playlistfilemodel.cpp \
    src/trackmodel.cpp \
    src/services.cpp

OTHER_FILES += \
    translations/*.ts \
    dbus/org.jupii.xml \
    qml/main.qml \
    qml/DevicesPage.qml \
    qml/CoverPage.qml \
    qml/Spacer.qml \
    qml/Notification.qml \
    qml/DeviceInfoPage.qml \
    qml/SettingsPage.qml \
    qml/AboutPage.qml \
    qml/PaddedLabel.qml \
    qml/ChangelogPage.qml \
    qml/LogItem.qml \
    qml/MediaRendererPage.qml \
    qml/PlayerPanel.qml \
    qml/MediaInfoPage.qml \
    qml/AddMediaPage.qml \
    qml/AlbumsPage.qml \
    qml/TracksPage.qml \
    qml/DoubleListItem.qml \
    qml/ArtistPage.qml \
    qml/PlaylistPage.qml \
    qml/SavePlaylistPage.qml

SAILFISHAPP_ICONS = 86x86 108x108 128x128 172x172 256x256

system(qdbusxml2cpp dbus/org.jupii.xml -a src/dbus_jupii_adaptor)
system(qdbusxml2cpp dbus/org.freedesktop.Tracker1.Steroids.xml -p src/dbus_tracker_inf)

CONFIG += sailfishapp_i18n
TRANSLATIONS += \
    translations/harbour-jupii.ts \
    translations/harbour-jupii-pl.ts \
    translations/harbour-jupii-nl.ts \
    translations/harbour-jupii-nl_BE.ts \
    translations/harbour-jupii-ru.ts \
    translations/harbour-jupii-de.ts \
    translations/harbour-jupii-sv.ts \
    translations/harbour-jupii-es.ts

images.files = images/*
images.path = /usr/share/$${TARGET}/images
INSTALLS += images

linux-g++-32 {
    lib.files = libav/i486/*
}
linux-g++ {
    lib.files = libav/arm/*
}

lib.path = /usr/share/$${TARGET}/lib
INSTALLS += lib

DEPENDPATH += $$INCLUDEPATH

OTHER_FILES += \
    rpm/$${TARGET}.yaml \
    rpm/$${TARGET}.changes.in \
    rpm/$${TARGET}.spec
