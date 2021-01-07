#
# SFOS:
#
# wget https://ftpmirror.gnu.org/libmicrohttpd/libmicrohttpd-latest.tar.gz
# tar -xf libmicrohttpd-latest.tar.gz
# cd libmicrohttpd-0.9.72
# chmod u+x ./configure
# sb2 -t SailfishOS-[target] ./configure --enable-shared --disable-doc --disable-examples --without-gnutls
# sb2 -t SailfishOS-[target] make
# cp -L ./src/microhttpd/.libs/libmicrohttpd.so.12 [dest]
#
# git clone -b master --single-branch https://framagit.org/medoc92/npupnp.git npupnp-master
# cd npupnp-master
# patch -p1 < [libs/libupnpp]/npupnp.patch
# chmod u+x ./autogen.sh
# sb2 -t SailfishOS-[target] ./autogen.sh
# sb2 -t SailfishOS-[target] ./configure --enable-shared --disable-static
# sb2 -t SailfishOS-[target] make
# cp -L .libs/libnpupnp.so.4 [dest]
#
# git clone -b master --single-branch https://framagit.org/medoc92/libupnpp.git libupnpp-master
# cd libupnpp-master
# chmod u+x ./autogen.sh
# sb2 -t SailfishOS-[target] ./autogen.sh
# sb2 -t SailfishOS-[target] ./configure --enable-shared --disable-static
# sb2 -t SailfishOS-[target] make
# cp -L .libs/libupnpp.so.9 [dest]
#

UPNPP_ROOT = $$PROJECTDIR/libs/libupnpp

INCLUDEPATH += $$UPNPP_ROOT/include

HEADERS += $$UPNPP_ROOT/include/npupnp/*.h
HEADERS += $$UPNPP_ROOT/include/libupnpp/*.h
HEADERS += $$UPNPP_ROOT/include/libupnpp/*.hxx
HEADERS += $$UPNPP_ROOT/include/libupnpp/control/*.hxx
HEADERS += $$UPNPP_ROOT/include/libupnpp/device/*.hxx

sailfish {
    x86 {
        LIBS += -L$$UPNPP_ROOT/build/i486 -l:libnpupnp.so.4 -l:libmicrohttpd.so.12 -l:libupnpp.so.9
        upnpp.files = $$UPNPP_ROOT/build/i486/*
    }

    arm {
        LIBS += -L$$UPNPP_ROOT/build/armv7hl -l:libnpupnp.so.4 -l:libmicrohttpd.so.12 -l:libupnpp.so.9
        upnpp.files = $$UPNPP_ROOT/build/armv7hl/*
    }

    upnpp.path = /usr/share/$${TARGET}/lib
    INSTALLS += upnpp
}

desktop {
    amd64 {
        LIBS += -L$$UPNPP_ROOT/build/amd64 \
                -l:libupnpp.a -l:libupnpputil.a -l:libnpupnp.a -l:libmicrohttpd.a -l:libexpat.a -l:libcurl.a
    }

    arm64 {
        LIBS += -L$$UPNPP_ROOT/build/arm64 \
                -l:libupnpp.a -l:libupnpputil.a -l:libnpupnp.a -l:libmicrohttpd.a -l:libexpat.a -l:libcurl.a
    }
}
