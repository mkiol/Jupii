TARGET = jupii

TEMPLATE = app

QT += gui qml quick quickcontrols2 svg

PROJECTDIR = $$PWD/..

CONFIG += desktop screencast link_pkgconfig qtquickcompiler
DEFINES += DESKTOP KIRIGAMI SCREENCAST QT_NO_URL_CAST_FROM_STRING
equals(FLATPAK, 1) {
    DEFINES += FLATPAK
}

include($$PROJECTDIR/core/jupii_core.pri)

OTHER_FILES += \
    $$PROJECTDIR/dbus/org.jupii.xml

RESOURCES += \
    jupii.qrc

OTHER_FILES += \
    translations/*.ts \
    qml/*.qml

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

IMAGES_DIR = $$PROJECTDIR/desktop/images
install_icons.path = $$PREFIX/share
install_icons.files = $$OUT_PWD/icons
install_icons.CONFIG = no_check_exist
install_icons.extra += mkdir -p $$OUT_PWD/icons/hicolor/32x32/apps;
install_icons.extra += cp $$IMAGES_DIR/$$TARGET-32.png $$OUT_PWD/icons/hicolor/32x32/apps/$${TARGET}.png;
install_icons.extra += mkdir -p $$OUT_PWD/icons/hicolor/48x48/apps;
install_icons.extra += cp $$IMAGES_DIR/$$TARGET-48.png $$OUT_PWD/icons/hicolor/48x48/apps/$${TARGET}.png;
install_icons.extra += mkdir -p $$OUT_PWD/icons/hicolor/64x64/apps;
install_icons.extra += cp $$IMAGES_DIR/$$TARGET-64.png $$OUT_PWD/icons/hicolor/64x64/apps/$${TARGET}.png;
install_icons.extra += mkdir -p $$OUT_PWD/icons/hicolor/72x72/apps;
install_icons.extra += cp $$IMAGES_DIR/$$TARGET-72.png $$OUT_PWD/icons/hicolor/72x72/apps/$${TARGET}.png;
install_icons.extra += mkdir -p $$OUT_PWD/icons/hicolor/96x96/apps;
install_icons.extra += cp $$IMAGES_DIR/$$TARGET-96.png $$OUT_PWD/icons/hicolor/96x96/apps/$${TARGET}.png;
install_icons.extra += mkdir -p $$OUT_PWD/icons/hicolor/128x128/apps;
install_icons.extra += cp $$IMAGES_DIR/$$TARGET-128.png $$OUT_PWD/icons/hicolor/128x128/apps/$${TARGET}.png;
install_icons.extra += mkdir -p $$OUT_PWD/icons/hicolor/150x150/apps;
install_icons.extra += cp $$IMAGES_DIR/$$TARGET-150.png $$OUT_PWD/icons/hicolor/150x150/apps/$${TARGET}.png;
install_icons.extra += mkdir -p $$OUT_PWD/icons/hicolor/192x192/apps;
install_icons.extra += cp $$IMAGES_DIR/$$TARGET-192.png $$OUT_PWD/icons/hicolor/192x192/apps/$${TARGET}.png;
install_icons.extra += mkdir -p $$OUT_PWD/icons/hicolor/256x256/apps;
install_icons.extra += cp $$IMAGES_DIR/$$TARGET-256.png $$OUT_PWD/icons/hicolor/256x256/apps/$${TARGET}.png;
install_icons.extra += mkdir -p $$OUT_PWD/icons/hicolor/512x512/apps;
install_icons.extra += cp $$IMAGES_DIR/$$TARGET-512.png $$OUT_PWD/icons/hicolor/512x512/apps/$${TARGET}.png;

install_appstream.path = $$PREFIX/share/metainfo
install_appstream.files = $$PROJECTDIR/desktop/$${TARGET}.metainfo.xml

install_license.path = $$PREFIX/share/$${TARGET}
install_license.files = $$PROJECTDIR/LICENSE

INSTALLS += install_bin install_desktop install_icons install_appstream install_license
