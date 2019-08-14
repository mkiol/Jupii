PUPNP_ROOT = $$PROJECTDIR/libs/pupnp

HEADERS += $$PUPNP_ROOT/upnp/*.h \
           $$PUPNP_ROOT/ixml/*.h \
           $$PUPNP_ROOT/threadutil/*.h

sailfish {
    x86 {
        LIBS += -L$$PROJECTDIR/libs/pupnp/build/i486/ -l:libupnp.so.6
        LIBS += -L$$PROJECTDIR/libs/pupnp/build/i486/ -l:libthreadutil.so.6
        LIBS += -L$$PROJECTDIR/libs/pupnp/build/i486/ -l:libixml.so.2
    }
    arm {
        LIBS += -L$$PROJECTDIR/libs/pupnp/build/armv7hl/ -l:libupnp.so.6
        LIBS += -L$$PROJECTDIR/libs/pupnp/build/armv7hl/ -l:libthreadutil.so.6
        LIBS += -L$$PROJECTDIR/libs/pupnp/build/armv7hl/ -l:libixml.so.2
    }
    x86: pupnp.files = $$PROJECTDIR/libs/pupnp/build/i486/*
    arm: pupnp.files = $$PROJECTDIR/libs/pupnp/build/armv7hl/*
    pupnp.path = /usr/share/$${TARGET}/lib
    INSTALLS += pupnp
}

desktop {
    LIBS += -L$$PROJECTDIR/libs/pupnp/build/amd64/ -l:libupnp.a
    LIBS += -L$$PROJECTDIR/libs/pupnp/build/amd64/ -l:libthreadutil.a
    LIBS += -L$$PROJECTDIR/libs/pupnp/build/amd64/ -l:libixml.a
}

#INCLUDEPATH += $$PUPNP_ROOT/libupnp-1.6.25
#INCLUDEPATH += $$PUPNP_ROOT/libupnp-1.6.25/upnp/src
#INCLUDEPATH += $$PUPNP_ROOT/libupnp-1.6.25/upnp/src/inc
#DEFINES += INCLUDE_DEVICE_APIS EXCLUDE_GENA=0
#SOURCES += $$files($$PUPNP_ROOT/libupnp-1.6.25/upnp/src/*.c, true)
#HEADERS += $$files($$PUPNP_ROOT/libupnp-1.6.25/upnp/src/*.h, true)
