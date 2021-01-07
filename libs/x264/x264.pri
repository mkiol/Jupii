#
# SFOS:
#
# git clone https://code.videolan.org/videolan/x264.git
# cd x264
# chmod u+x ./configure
# sb2 -t SailfishOS-[target] ./configure --enable-pic --enable-shared --enable-strip
# sb2 -t SailfishOS-[target] make
# cp -L libx264.so.161 [dest]
#

X264_ROOT = $$PROJECTDIR/libs/x264

INCLUDEPATH += $$X264_ROOT/include

LIBS += -ldl

sailfish {
    x86 {
        LIBS += -L$$X264_ROOT/build/i486 -l:libx264.so.161
        libx264.files = $$X264_ROOT/build/i486/*
    }

    arm {
        LIBS += -L$$X264_ROOT/build/armv7hl -l:libx264.so.161
        libx264.files = $$X264_ROOT/build/armv7hl/*
    }

    libx264.path = /usr/share/$${TARGET}/lib
    INSTALLS += libx264
}

desktop {     
    amd64 {
        LIBS += -L$$X264_ROOT/build/amd64 \
                -l:libx264.a
    }

    arm64 {
        LIBS += -L$$X264_ROOT/build/arm64 \
                -l:libx264.a
    }
}
