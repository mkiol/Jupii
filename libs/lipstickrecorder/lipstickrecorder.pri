LIPSTICKRECORDER_ROOT = $$PROJECTDIR/libs/lipstickrecorder

# needed for qpa/qplatformnativeinterface.h
QT += gui-private

CONFIG += link_pkgconfig wayland-scanner
PKGCONFIG += wayland-client
WAYLANDCLIENTSOURCES += $$LIPSTICKRECORDER_ROOT/protocol/lipstick-recorder.xml

INCLUDEPATH += /usr/include
INCLUDEPATH += $$LIPSTICKRECORDER_ROOT

SOURCES += \
    $$LIPSTICKRECORDER_ROOT/recorder.cpp \
    $$LIPSTICKRECORDER_ROOT/frameevent.cpp \
    $$LIPSTICKRECORDER_ROOT/buffer.cpp

HEADERS += \
    $$LIPSTICKRECORDER_ROOT/recorder.h \
    $$LIPSTICKRECORDER_ROOT/frameevent.h \
    $$LIPSTICKRECORDER_ROOT/buffer.h
