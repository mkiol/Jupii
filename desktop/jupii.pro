TARGET = jupii

TEMPLATE = app

CONFIG += c++11 json no_lflags_merge object_parallel_to_source
QT += core gui widgets network dbus sql multimedia xml

PROJECTDIR = $$PWD/..

CONFIG += desktop screencast link_pkgconfig
DEFINES += DESKTOP SCREENCAST

include($$PROJECTDIR/core/jupii_core.pri)

INCLUDEPATH += src

HEADERS += \
    src/filedownloader.h \
    src/mainwindow.h \
    src/settingsdialog.h \
    src/addurldialog.h

SOURCES += \
    src/mainwindow.cpp \
    src/filedownloader.cpp \
    src/settingsdialog.cpp \
    src/addurldialog.cpp

FORMS += \
    src/mainwindow.ui \
    src/settingsdialog.ui \
    src/addurldialog.ui

OTHER_FILES += \
    $$PROJECTDIR/dbus/org.jupii.xml \
    packages/*.spec \
    packages/*.sh \
    packages/control \
    packages/PKGINFO

RESOURCES += \
    jupii.qrc

####################
# files to install #
####################

isEmpty(PREFIX) {
    PREFIX = /usr/local
}

message(Install prefix is $$PREFIX)

install_bin.path = $$PREFIX/bin
install_bin.files = $$OUT_PWD/$$TARGET
install_bin.CONFIG = no_check_exist executable

install_desktop.path = $$PREFIX/share/applications
install_desktop.files = $$PROJECTDIR/desktop/$${TARGET}.desktop

install_icons.path = $$PREFIX/share
install_icons.files = $$OUT_PWD/icons
install_icons.CONFIG = no_check_exist
install_icons.extra += mkdir -p $$OUT_PWD/icons/hicolor/48x48/apps;
install_icons.extra += cp $$PROJECTDIR/desktop/images/$$TARGET-48.png $$OUT_PWD/icons/hicolor/48x48/apps/$${TARGET}.png;
install_icons.extra += mkdir -p $$OUT_PWD/icons/hicolor/64x64/apps;
install_icons.extra += cp $$PROJECTDIR/desktop/images/$$TARGET-64.png $$OUT_PWD/icons/hicolor/64x64/apps/$${TARGET}.png;
install_icons.extra += mkdir -p $$OUT_PWD/icons/hicolor/128x128/apps;
install_icons.extra += cp $$PROJECTDIR/desktop/images/$$TARGET-128.png $$OUT_PWD/icons/hicolor/128x128/apps/$${TARGET}.png;

install_appstream.path = $$PREFIX/share/metainfo
install_appstream.files = $$PROJECTDIR/desktop/$${TARGET}.metainfo.xml

INSTALLS += install_bin install_desktop install_icons install_appstream
