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
    x86 {
        LIBS += -L$$LAME_ROOT/build/i486/ -l:libmp3lame.so.0
        liblame.files = $$LAME_ROOT/build/i486/*
    }

    arm {
        LIBS += -L$$LAME_ROOT/build/armv7hl/ -l:libmp3lame.so.0
        liblame.files = $$LAME_ROOT/build/armv7hl/*
    }

    liblame.path = /usr/share/$${TARGET}/lib
    INSTALLS += liblame
}

desktop {
    amd64 {
        LIBS += -L$$LAME_ROOT/build/amd64 \
                -l:libmp3lame.a
    }

    arm64 {
        LIBS += -L$$LAME_ROOT/build/arm64 \
                -l:libmp3lame.a
    }
}
