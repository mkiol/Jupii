# FFMPEG URL="https://ffmpeg.org/releases/ffmpeg-5.1.1.tar.gz"

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
    LIBS += -L$${FFMPEG_ROOT}/build/sfos-$${ARCH_PREFIX} \
            -l:libavutil.so.57 \
            -l:libavcodec.so.59 \
            -l:libavformat.so.59 \
            -l:libavdevice.so.59 \
            -l:libswresample.so.4 \
            -l:libswscale.so.6
    libffmpeg.files = $${FFMPEG_ROOT}/build/sfos-$${ARCH_PREFIX}/*

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
