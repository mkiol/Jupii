TARGET = harbour-jupii

CONFIG += c++11 sailfishapp json no_lflags_merge object_parallel_to_source

QT += dbus sql multimedia xml

PKGCONFIG += mlite5

linux-g++-32: CONFIG += x86
linux-g++: CONFIG += arm

PROJECTDIR = $$PWD/..

INCLUDEPATH += /usr/include/c++/7

CONFIG += sailfish ffmpeg
DEFINES += SAILFISH

ffmpeg {
    DEFINES += FFMPEG
    INCLUDEPATH += $$PWD/libav/src

    x86: lib.files = libav/i486/*
    arm: lib.files = libav/arm/*
    lib.path = /usr/share/$${TARGET}/lib
    INSTALLS += lib

    x86 {
        LIBS += -L$$PWD/libav/i486/ -l:libavutil.so.55
        LIBS += -L$$PWD/libav/i486/ -l:libavformat.so.57
        LIBS += -L$$PWD/libav/i486/ -l:libavcodec.so.57
        LIBS += -L$$PWD/libav/i486/ -l:libavresample.so.3
    }

    arm {
        LIBS += -L$$PWD/libav/arm/ -l:libavutil.so.55
        LIBS += -L$$PWD/libav/arm/ -l:libavformat.so.57
        LIBS += -L$$PWD/libav/arm/ -l:libavcodec.so.57
        LIBS += -L$$PWD/libav/arm/ -l:libavresample.so.3
    }
}

include($$PROJECTDIR/libs/qhttpserver/qhttpserver.pri)
include($$PROJECTDIR/libs/libupnp/libupnp.pri)
include($$PROJECTDIR/libs/libupnpp/libupnpp.pri)
include($$PROJECTDIR/libs/taglib/taglib.pri)
include($$PROJECTDIR/core/jupii_core.pri)

INCLUDEPATH += src

OTHER_FILES += \
    translations/*.ts \
    $$PROJECTDIR/dbus/org.jupii.xml \
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
    qml/SavePlaylistPage.qml \
    qml/DBusVolumeAgent.qml \
    qml/AddUrlPage.qml \
    qml/Tip.qml \
    qml/SomafmPage.qml \
    qml/GpodderPage.qml \
    qml/GpodderEpisodesPage.qml \
    qml/SearchPageHeader.qml \
    qml/SearchDialogHeader.qml \
    qml/IcecastPage.qml

SAILFISHAPP_ICONS = 86x86 108x108 128x128 172x172 256x256

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

somafm.files = somafm/*
somafm.path = /usr/share/$${TARGET}/somafm
INSTALLS += somafm

DEPENDPATH += $$INCLUDEPATH

OTHER_FILES += \
    rpm/$${TARGET}.yaml \
    rpm/$${TARGET}.changes.in \
    rpm/$${TARGET}.spec
