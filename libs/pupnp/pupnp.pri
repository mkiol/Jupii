PUPNP_ROOT = $$PROJECTDIR/libs/pupnp

INCLUDEPATH += $$PUPNP_ROOT/include \
               $$PUPNP_ROOT/include/upnp

sailfish {
    x86 {
        LIBS += -L$$PUPNP_ROOT/build/i486 -l:libupnp.so.6 -l:libthreadutil.so.6 -l:libixml.so.2
        pupnp.files = $$PUPNP_ROOT/build/i486/*
    }

    arm {
        LIBS += -L$$PUPNP_ROOT/build/armv7hl -l:libupnp.so.6 -l:libthreadutil.so.6 -l:libixml.so.2
        pupnp.files = $$PUPNP_ROOT/build/armv7hl/*
    }

    pupnp.path = /usr/share/$${TARGET}/lib
    INSTALLS += pupnp
}

desktop {
    LIBS += -L$$PUPNP_ROOT/build/amd64 -l:libupnp.a -l:libthreadutil.a -l:libixml.a
}
