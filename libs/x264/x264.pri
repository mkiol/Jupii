#
# SFOS:
#
# git clone https://code.videolan.org/videolan/x264.git
# cd x264
# chmod u+x ./configure
# sb2 -t SailfishOS-[target] ./configure --enable-pic --enable-shared --enable-strip
# sb2 -t SailfishOS-[target] make
# cp -L libx264.so.164 [dest]
#

X264_ROOT = $$PROJECTDIR/libs/x264

INCLUDEPATH += $$X264_ROOT/include

LIBS += -ldl

sailfish {
    LIBS += -L$${X264_ROOT}/build/sfos-$${ARCH_PREFIX} -l:libx264.so.164
    libx264.files = $${X264_ROOT}/build/sfos-$${ARCH_PREFIX}/*
    libx264.path = /usr/share/$${TARGET}/lib
    INSTALLS += libx264
}

desktop {     
    LIBS += -L$${X264_ROOT}/build/$${ARCH_PREFIX} \
            -l:libx264.a
}
