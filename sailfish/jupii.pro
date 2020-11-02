TARGET = harbour-jupii

CONFIG += link_pkgconfig

linux-g++-32: CONFIG += x86
linux-g++: CONFIG += arm

PROJECTDIR = $$PWD/..

INCLUDEPATH += /usr/include/c++/8.3.0
arm: INCLUDEPATH += /usr/include/c++/8.3.0/armv7hl-meego-linux-gnueabi

CONFIG += sailfish screencast
DEFINES += SAILFISH SCREENCAST

include($$PROJECTDIR/core/jupii_core.pri)

INCLUDEPATH += src

HEADERS += \
    src/resourcehandler.h

SOURCES += \
    src/resourcehandler.cpp

OTHER_FILES += \
    translations/*.ts \
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
    qml/PlayerPanel.qml \
    qml/MediaInfoPage.qml \
    qml/AddMediaPage.qml \
    qml/AlbumsPage.qml \
    qml/TracksPage.qml \
    qml/DoubleListItem.qml \
    qml/ArtistPage.qml \
    qml/PlaylistPage.qml \
    qml/SavePlaylistPage.qml \
    qml/AddUrlPage.qml \
    qml/Tip.qml \
    qml/SomafmPage.qml \
    qml/FosdemYearsPage.qml \
    qml/FosdemPage.qml \
    qml/GpodderEpisodesPage.qml \
    qml/SearchPageHeader.qml \
    qml/SearchDialogHeader.qml \
    qml/IcecastPage.qml \
    qml/DirPage.qml \
    qml/RecPage.qml \
    qml/SimpleFavListItem.qml \
    qml/SimpleListItem.qml \
    qml/UpnpCDirDevicesPage.qml \
    qml/UpnpCDirPage.qml \
    qml/PlayQueuePage.qml \
    qml/BcPage.qml \
    qml/TuneinPage.qml \
    qml/InteractionHintLabel_.qml

SAILFISHAPP_ICONS = 86x86 108x108 128x128 172x172 256x256

TRANSLATION_SOURCE_DIRS += $$PROJECTDIR/core
CONFIG += sailfishapp_i18n_include_obsolete
TRANSLATIONS += \
    translations/harbour-jupii-en.ts \
    translations/harbour-jupii-pl.ts \
    translations/harbour-jupii-nl.ts \
    translations/harbour-jupii-nl_BE.ts \
    translations/harbour-jupii-ru.ts \
    translations/harbour-jupii-de.ts \
    translations/harbour-jupii-sv.ts \
    translations/harbour-jupii-es.ts \
    translations/harbour-jupii-zh_CN.ts \
    translations/harbour-jupii-sl_SI.ts \
    translations/harbour-jupii-it.ts
include(sailfishapp_i18n.pri)

images.files = images/*
images.path = /usr/share/$${TARGET}/images
INSTALLS += images

DEPENDPATH += $$INCLUDEPATH

PKGCONFIG += \
    audioresource \
    nemonotifications-qt5 \
    mlite5

LIBS += -ldl

include(sailfishapp.pri)
