TARGET = jupii

TEMPLATE = app

QT += gui qml quick quickcontrols2 widgets

PROJECTDIR = $${PWD}/..

contains(QT_ARCH, i386){
   message("Compiling for x86")
   CONFIG += x86
   DEFINES += X86
} else {
   contains(QT_ARCH, x86_64) {
       message("Compiling for amd64")
       CONFIG += amd64
       DEFINES += X86
   } else {
       contains(QT_ARCH, arm){
           message("Compiling for arm")
           CONFIG += arm
           DEFINES += ARM
       } else {
            contains(QT_ARCH, arm64) {
                message("Compiling for arm64")
                CONFIG += arm64
                DEFINES += ARM
            }
       }
   }
}

CONFIG += desktop screencast link_pkgconfig qtquickcompiler
DEFINES += DESKTOP KIRIGAMI SCREENCAST QT_NO_URL_CAST_FROM_STRING
equals(FLATPAK, 1) {
    DEFINES += FLATPAK
}

include($${PROJECTDIR}/core/jupii_core.pri)

OTHER_FILES += \
    $${PROJECTDIR}/dbus/org.jupii.xml \
    $${PROJECTDIR}/desktop/org.mkiol.Jupii.json \
    $${PROJECTDIR}/desktop/jupii.metainfo.xml \
    $${PROJECTDIR}/desktop/jupii.desktop

################
# translations #
################

TRANSLATION_SOURCE_DIRS += $${PROJECTDIR}/core \
                           $${PROJECTDIR}/sailfish/qml \
                           $${PROJECTDIR}/sailfish/src \
                           $${PROJECTDIR}/desktop/qml \
                           $${PROJECTDIR}/desktop/src
TRANSLATIONS_DIR = $${PROJECTDIR}/translations
TRANSLATIONS += \
    $${TRANSLATIONS_DIR}/jupii-en.ts \
    $${TRANSLATIONS_DIR}/jupii-pl.ts \
    $${TRANSLATIONS_DIR}/jupii-nl.ts \
    $${TRANSLATIONS_DIR}/jupii-nl_BE.ts \
    $${TRANSLATIONS_DIR}/jupii-ru.ts \
    $${TRANSLATIONS_DIR}/jupii-de.ts \
    $${TRANSLATIONS_DIR}/jupii-sv.ts \
    $${TRANSLATIONS_DIR}/jupii-es.ts \
    $${TRANSLATIONS_DIR}/jupii-zh_CN.ts \
    $${TRANSLATIONS_DIR}/jupii-sl_SI.ts \
    $${TRANSLATIONS_DIR}/jupii-it.ts

QT_BIN_DIR = "$$[QT_INSTALL_PREFIX]/bin"
exists("$${QT_BIN_DIR}/lrelease-qt5") {
    LRELEASE_BIN = "$${QT_BIN_DIR}/lrelease-qt5"
} else {
    LRELEASE_BIN = "$${QT_BIN_DIR}/lrelease"
}
exists("$${QT_BIN_DIR}/lupdate-qt5") {
    LUPDATE_BIN = "$${QT_BIN_DIR}/lupdate-qt5"
} else {
    LUPDATE_BIN = "$${QT_BIN_DIR}/lupdate"
}

system(mkdir -p $${TRANSLATIONS_DIR})
system(mkdir -p translations)
for(dir, TRANSLATION_SOURCE_DIRS) {
    exists($$dir) {
        TRANSLATION_SOURCES += $$clean_path($$dir)
    }
}
system("$$LUPDATE_BIN" $${TRANSLATION_SOURCES} -ts $$TRANSLATIONS)
for(t, TRANSLATIONS) {
    qmfile = $$replace(t, \.ts, .qm)
    system("$$LRELEASE_BIN" "$$t" -qm translations/$$basename(qmfile))
}

RESOURCES += \
    jupii.qrc

OTHER_FILES += \
    qml/*.qml

####################
# files to install #
####################

isEmpty(PREFIX) {
    PREFIX = /usr/local
}

message(Install prefix is $$PREFIX)

install_bin.path = $${PREFIX}/bin
install_bin.files = $${OUT_PWD}/$${TARGET}
install_bin.CONFIG = no_check_exist executable

install_desktop.path = $${PREFIX}/share/applications
install_desktop.files = $${PROJECTDIR}/desktop/$${TARGET}.desktop

IMAGES_DIR = $${PROJECTDIR}/desktop/images
install_icons.path = $${PREFIX}/share
install_icons.files = $${OUT_PWD}/icons
install_icons.CONFIG = no_check_exist
install_icons.extra += mkdir -p $${OUT_PWD}/icons/hicolor/32x32/apps;
install_icons.extra += cp $$IMAGES_DIR/$${TARGET}-32.png $${OUT_PWD}/icons/hicolor/32x32/apps/$${TARGET}.png;
install_icons.extra += mkdir -p $${OUT_PWD}/icons/hicolor/48x48/apps;
install_icons.extra += cp $$IMAGES_DIR/$${TARGET}-48.png $${OUT_PWD}/icons/hicolor/48x48/apps/$${TARGET}.png;
install_icons.extra += mkdir -p $${OUT_PWD}/icons/hicolor/64x64/apps;
install_icons.extra += cp $$IMAGES_DIR/$${TARGET}-64.png $${OUT_PWD}/icons/hicolor/64x64/apps/$${TARGET}.png;
install_icons.extra += mkdir -p $${OUT_PWD}/icons/hicolor/72x72/apps;
install_icons.extra += cp $$IMAGES_DIR/$${TARGET}-72.png $${OUT_PWD}/icons/hicolor/72x72/apps/$${TARGET}.png;
install_icons.extra += mkdir -p $${OUT_PWD}/icons/hicolor/96x96/apps;
install_icons.extra += cp $$IMAGES_DIR/$${TARGET}-96.png $${OUT_PWD}/icons/hicolor/96x96/apps/$${TARGET}.png;
install_icons.extra += mkdir -p $${OUT_PWD}/icons/hicolor/128x128/apps;
install_icons.extra += cp $$IMAGES_DIR/$${TARGET}-128.png $${OUT_PWD}/icons/hicolor/128x128/apps/$${TARGET}.png;
install_icons.extra += mkdir -p $${OUT_PWD}/icons/hicolor/150x150/apps;
install_icons.extra += cp $$IMAGES_DIR/$${TARGET}-150.png $${OUT_PWD}/icons/hicolor/150x150/apps/$${TARGET}.png;
install_icons.extra += mkdir -p $${OUT_PWD}/icons/hicolor/192x192/apps;
install_icons.extra += cp $$IMAGES_DIR/$${TARGET}-192.png $${OUT_PWD}/icons/hicolor/192x192/apps/$${TARGET}.png;
install_icons.extra += mkdir -p $${OUT_PWD}/icons/hicolor/256x256/apps;
install_icons.extra += cp $$IMAGES_DIR/$${TARGET}-256.png $${OUT_PWD}/icons/hicolor/256x256/apps/$${TARGET}.png;
install_icons.extra += mkdir -p $${OUT_PWD}/icons/hicolor/512x512/apps;
install_icons.extra += cp $$IMAGES_DIR/$${TARGET}-512.png $${OUT_PWD}/icons/hicolor/512x512/apps/$${TARGET}.png;

install_appstream.path = $$PREFIX/share/metainfo
install_appstream.files = $$PROJECTDIR/desktop/$${TARGET}.metainfo.xml

install_license.path = $$PREFIX/share/$${TARGET}
install_license.files = $$PROJECTDIR/LICENSE

INSTALLS += install_bin install_desktop install_icons install_appstream install_license

DISTFILES += \
    ../translations/jupii-de.ts \
    ../translations/jupii-en.ts \
    ../translations/jupii-es.ts \
    ../translations/jupii-it.ts \
    ../translations/jupii-nl.ts \
    ../translations/jupii-nl_BE.ts \
    ../translations/jupii-pl.ts \
    ../translations/jupii-ru.ts \
    ../translations/jupii-sl_SI.ts \
    ../translations/jupii-sv.ts \
    ../translations/jupii-zh_CN.ts
