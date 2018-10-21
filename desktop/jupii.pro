TARGET = jupii

TEMPLATE = app

CONFIG += c++11 json no_lflags_merge object_parallel_to_source
QT += core gui widgets network dbus sql

PROJECTDIR = $$PWD/..

CONFIG += desktop
DEFINES += DESKTOP

ffmpeg {
    DEFINES += FFMPEG
    LIBS += -lavutil -lavformat -lavcodec -lavresample
    INCLUDEPATH += /usr/include/ffmpeg
}

include($$PROJECTDIR/libs/qhttpserver/qhttpserver.pri)
include($$PROJECTDIR/libs/libupnp/libupnp.pri)
include($$PROJECTDIR/libs/libupnpp/libupnpp.pri)
include($$PROJECTDIR/libs/taglib/taglib.pri)
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
    packages/*.sh

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    jupii.qrc
