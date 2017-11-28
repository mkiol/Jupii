TARGET = harbour-jupii

CONFIG += c++11 sailfishapp object_parallel_to_source json dbus

PKGCONFIG += mlite5

INCLUDEPATH += /usr/include/c++/7

DEFINES += DEBUG

LIBS += -lpthread -lcurl -lexpat

include(libupnp/libupnp.pri)
include(libupnpp/libupnpp.pri)
include(qhttpserver/qhttpserver.pri)
include(taglib/taglib.pri)

HEADERS += \
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
    src/playlistmodel.h

SOURCES += \
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
    src/playlistmodel.cpp

OTHER_FILES += \
    rpm/jupii.in \
    rpm/jupii.yaml \
    translations/*.ts \
    harbour-jupii.desktop

DISTFILES += \
    qml/main.qml \
    qml/DevicesPage.qml \
    qml/CoverPage.qml \
    qml/Spacer.qml \
    qml/Notification.qml \
    qml/DeviceItem.qml \
    qml/DeviceInfoPage.qml \
    qml/AttValue.qml \
    qml/SettingsPage.qml \
    qml/AboutPage.qml \
    qml/PaddedLabel.qml \
    qml/ChangelogPage.qml \
    qml/LogItem.qml \
    qml/MediaRendererPage.qml \
    qml/PlayerPanel.qml \
    qml/MediaInfoPage.qml \
    qml/AddMediaPage.qml

SAILFISHAPP_ICONS = 86x86 108x108 128x128 172x172 256x256

# to disable building translations every time, comment out the
# following CONFIG line
#CONFIG += sailfishapp_i18n

# German translation is enabled as an example. If you aren't
# planning to localize your app, remember to comment out the
# following TRANSLATIONS line. And also do not forget to
# modify the localized app name in the the .desktop file.
#TRANSLATIONS += translations/jupii-de.ts

images.files = images/*
images.path = /usr/share/$${TARGET}/images
INSTALLS += images
