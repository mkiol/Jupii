#
# SFOS:
#
# tar -xf libupnp-1.6.25.tar.bz2
# cd libupnp-1.6.25
# chmod u+x ./configure
# sb2 -t SailfishOS-[target] ./configure --enable-shared --disable-static --disable-samples --disable-blocking-tcp-connections --disable-debug
# sb2 -t SailfishOS-[target] make
# cp -L upnp/.libs/libupnp.so.6 [dest]
# cp -L ixml/.libs/libixml.so.2 [dest]
# cp -L threadutil/.libs/libthreadutil.so.6 [dest]
#

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
