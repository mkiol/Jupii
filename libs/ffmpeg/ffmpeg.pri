FFMPEG_ROOT = $$PROJECTDIR/libs/ffmpeg

INCLUDEPATH += $$FFMPEG_ROOT/include

HEADERS += \
    $$FFMPEG_ROOT/include/libavcodec/*.h \
    $$FFMPEG_ROOT/include/libavdevice/*.h \
    $$FFMPEG_ROOT/include/libavfilter/*.h \
    $$FFMPEG_ROOT/include/libavformat/*.h \
    $$FFMPEG_ROOT/include/libavutil/*.h \
    $$FFMPEG_ROOT/include/libswresample/*.h \
    $$FFMPEG_ROOT/include/libswscale/*.h

sailfish {
    PKGCONFIG += zlib
    LIBS += -lbz2
    CONFIG(debug, debug|release) {
        LIBS += -L$${FFMPEG_ROOT}/build/sfos-$${ARCH_PREFIX}-debug \
                -l:libavutil.so.56 \
                -l:libavcodec.so.58 \
                -l:libavformat.so.58 \
                -l:libavdevice.so.58 \
                -l:libswresample.so.3 \
                -l:libswscale.so.5
        libffmpeg.files = $${FFMPEG_ROOT}/build/sfos-$${ARCH_PREFIX}-debug/*
    } else {
        LIBS += -L$${FFMPEG_ROOT}/build/sfos-$${ARCH_PREFIX} \
                -l:libavutil.so.56 \
                -l:libavcodec.so.58 \
                -l:libavformat.so.58 \
                -l:libavdevice.so.58 \
                -l:libswresample.so.3 \
                -l:libswscale.so.5
        libffmpeg.files = $${FFMPEG_ROOT}/build/sfos-$${ARCH_PREFIX}/*
    }

    libffmpeg.path = /usr/share/$${TARGET}/lib
    INSTALLS += libffmpeg
}

desktop {
    PKGCONFIG += zlib xcb xcb-shm xcb-xfixes xcb-shape x11 libva \
                 libva-drm libva-x11 vdpau
    LIBS += -L$${FFMPEG_ROOT}/build/$${ARCH_PREFIX} \
            -l:libswscale.a \
            -l:libavdevice.a \
            -l:libavformat.a \
            -l:libavfilter.a \
            -l:libavcodec.a \
            -l:libavutil.a \
            -l:libswresample.a
    LIBS += -lbz2 # Ubuntu bzip2 package does not provide pkgconfig, so adding manually
}
