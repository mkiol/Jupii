TARGET = harbour-jupii

CONFIG += c++11 sailfishapp object_parallel_to_source json
QT += dbus

PKGCONFIG += mlite5

INCLUDEPATH += /usr/include/c++/7

DEFINES += Q_OS_SAILFISH

LIBS += -lpthread -lcurl -lexpat

include(libupnp/libupnp.pri)
include(libupnpp/libupnpp.pri)
include(qhttpserver/qhttpserver.pri)
include(taglib/taglib.pri)

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
    src/trackmodel.h

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
    src/trackmodel.cpp

OTHER_FILES += \
    rpm/$${TARGET}.yaml \
    rpm/$${TARGET}.changes.in \
    $${TARGET}.desktop \
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
    qml/AddMediaPage.qml

SAILFISHAPP_ICONS = 86x86 108x108 128x128 172x172 256x256

system(qdbusxml2cpp dbus/org.jupii.xml -a src/dbus_jupii_adaptor)
system(qdbusxml2cpp dbus/org.freedesktop.Tracker1.Steroids.xml -p src/dbus_tracker_inf)

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

DISTFILES += \
    qml/AlbumsPage.qml \
    qml/TracksPage.qml \
    qml/DoubleListItem.qml \
    qml/ArtistPage.qml
