TARGET = harbour-jupii

CONFIG += link_pkgconfig

contains(QT_ARCH, i386){
   message("Compiling for x86")
   CONFIG += x86
   DEFINES += X86
   ARCH_PREFIX = x86
} else {
   contains(QT_ARCH, x86_64) {
       message("Compiling for amd64")
       CONFIG += amd64
       DEFINES += X86
       ARCH_PREFIX = amd64
   } else {
       contains(QT_ARCH, arm){
           message("Compiling for arm")
           CONFIG += arm
           DEFINES += ARM
           ARCH_PREFIX = arm
       } else {
            contains(QT_ARCH, arm64) {
                message("Compiling for arm64")
                CONFIG += arm64
                DEFINES += ARM
                ARCH_PREFIX = arm64
            }
       }
   }
}

PROJECTDIR = $$PWD/..

INCLUDEPATH += /usr/include/c++/8.3.0
arm: INCLUDEPATH += /usr/include/c++/8.3.0/armv7hl-meego-linux-gnueabi
arm64: INCLUDEPATH += /usr/include/c++/8.3.0/aarch64-meego-linux-gnu

CONFIG += sailfish screencast
DEFINES += SAILFISH SCREENCAST

include($${PROJECTDIR}/src/jupii.pri)

INCLUDEPATH += src

HEADERS += \
    src/resourcehandler.h

SOURCES += \
    src/resourcehandler.cpp

OTHER_FILES += \
    sailjail/*.permissions \
    ../translations/*.ts \
    qml/*.qml \
    rpm/*.spec \
    *.desktop

################
# translations #
################

TRANSLATION_SOURCE_DIRS += $${PROJECTDIR}/src \
                           $${PROJECTDIR}/sailfish/qml \
                           $${PROJECTDIR}/sailfish/src \
                           $${PROJECTDIR}/desktop/qml \
                           $${PROJECTDIR}/desktop/src
#CONFIG += sailfishapp_i18n_include_obsolete
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
include(sailfishapp_i18n.pri)

##########
# images #
##########

SAILFISHAPP_ICONS = 86x86 108x108 128x128 172x172 256x256

images.files = images/*
images.path = /usr/share/$${TARGET}/images
INSTALLS += images

############
# sailjail #
############

sailjail.files = sailjail/*
sailjail.path = /etc/sailjail/permissions
INSTALLS += sailjail

DEPENDPATH += $$INCLUDEPATH

PKGCONFIG += \
    audioresource \
    nemonotifications-qt5 \
    mlite5

LIBS += -ldl

include(sailfishapp.pri)
