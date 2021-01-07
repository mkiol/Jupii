#
# SFOS:
#
# tar -xf gumbo-parser-0.10.1.tar.gz
# cd gumbo-parser-0.10.1
# sb2 -t SailfishOS-[target] -m sdk-install -R zypper in libtool
# sb2 -t SailfishOS-[target] ./autogen.sh
# sb2 -t SailfishOS-[target] ./configure --disable-static --enable-shared
# sb2 -t SailfishOS-[target] make
# cp -L .libs/libgumbo.so.1 [dest]
#

GUMBO_ROOT = $$PROJECTDIR/libs/gumbo

INCLUDEPATH += $$GUMBO_ROOT/include
HEADERS += \
    $$GUMBO_ROOT/include/gumbo.h \
    $$GUMBO_ROOT/include/tag_enum.h

sailfish {
    x86 {
        LIBS += -L$$GUMBO_ROOT/build/i486/ -l:libgumbo.so.1
        gumbo.files = $$GUMBO_ROOT/build/i486/*
    }

    arm {
        LIBS += -L$$GUMBO_ROOT/build/armv7hl/ -l:libgumbo.so.1
        gumbo.files = $$GUMBO_ROOT/build/armv7hl/*
    }

    gumbo.path = /usr/share/$${TARGET}/lib
    INSTALLS += gumbo
}

desktop {
    amd64 {
        LIBS += -L$$GUMBO_ROOT/build/amd64 \
                -l:libgumbo.a
    }

    arm64 {
        LIBS += -L$$GUMBO_ROOT/build/arm64 \
                -l:libgumbo.a
    }
}
