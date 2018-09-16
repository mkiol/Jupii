TARGET = jupii

TEMPLATE = app

CONFIG += c++11 json no_lflags_merge object_parallel_to_source
QT += core gui widgets network dbus

PROJECTDIR = $$PWD/..

CONFIG += desktop
DEFINES += DESKTOP

ffmpeg {
    DEFINES += FFMPEG
    LIBS += -lavutil -lavformat -lavcodec -lavresample
    INCLUDEPATH += /usr/include/ffmpeg
}

include($$PROJECTDIR/qhttpserver/qhttpserver.pri)
include($$PROJECTDIR/libupnp/libupnp.pri)
include($$PROJECTDIR/libupnpp/libupnpp.pri)
include($$PROJECTDIR/taglib/taglib.pri)
include($$PROJECTDIR/core_src/jupii_core.pri)

INCLUDEPATH += src

HEADERS += \
    src/filedownloader.h \
    src/mainwindow.h \
    src/settingsdialog.h

SOURCES += \
    src/mainwindow.cpp \
    src/filedownloader.cpp \
    src/settingsdialog.cpp

FORMS += \
    src/mainwindow.ui \
    src/settingsdialog.ui

OTHER_FILES += \
    $$PROJECTDIR/dbus/org.jupii.xml \
    packages/*.spec \
    packages/*.sh

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    jupii.qrc
