UPNPP_ROOT = $$PROJECTDIR/libs/libupnpp

INCLUDEPATH += $$UPNPP_ROOT/include

HEADERS += $$UPNPP_ROOT/include/npupnp/*.h
HEADERS += $$UPNPP_ROOT/include/libupnpp/*.h
HEADERS += $$UPNPP_ROOT/include/libupnpp/*.hxx
HEADERS += $$UPNPP_ROOT/include/libupnpp/control/*.hxx
HEADERS += $$UPNPP_ROOT/include/libupnpp/device/*.hxx

sailfish {
    CONFIG(debug, debug|release) {
        LIBS += -L$${UPNPP_ROOT}/build/sfos-$${ARCH_PREFIX}-debug -l:libnpupnp.so.4 -l:libmicrohttpd.so.12 -l:libupnpp.so.11
        upnpp.files = $${UPNPP_ROOT}/build/sfos-$${ARCH_PREFIX}-debug/*
    } else {
        LIBS += -L$${UPNPP_ROOT}/build/sfos-$${ARCH_PREFIX} -l:libnpupnp.so.4 -l:libmicrohttpd.so.12 -l:libupnpp.so.11
        upnpp.files = $${UPNPP_ROOT}/build/sfos-$${ARCH_PREFIX}/*
    }

    upnpp.path = /usr/share/$${TARGET}/lib
    INSTALLS += upnpp
}

desktop {
    LIBS += -L$${UPNPP_ROOT}/build/$${ARCH_PREFIX} \
            -l:libupnpp.a -l:libupnpputil.a -l:libnpupnp.a -l:libmicrohttpd.a -l:libexpat.a -l:libcurl.a
}
