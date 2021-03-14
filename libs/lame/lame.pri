#
# SFOS:
#
# tar -xf lame-3.100.tar.gz
# cd lame-3.100
# chmod u+x ./configure
# sb2 -t SailfishOS-[target] ./configure --with-pic --enable-shared --disable-static --disable-frontend
# sb2 -t SailfishOS-[target] make
# cp -L libmp3lame/.libs/libmp3lame.so.0 [dest]
#

LAME_ROOT = $$PROJECTDIR/libs/lame

INCLUDEPATH += $$LAME_ROOT/include
HEADERS += $$LAME_ROOT/include/*.h

sailfish {
    LIBS += -L$${LAME_ROOT}/build/sfos-$${ARCH_PREFIX}/ -l:libmp3lame.so.0
    liblame.files = $${LAME_ROOT}/build/sfos-$${ARCH_PREFIX}/*
    liblame.path = /usr/share/$${TARGET}/lib
    INSTALLS += liblame
}

desktop {
    LIBS += -L$${LAME_ROOT}/build/$${ARCH_PREFIX} \
            -l:libmp3lame.a
}
