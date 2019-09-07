FFMPEG_ROOT = $$PROJECTDIR/libs/ffmpeg

INCLUDEPATH += $$FFMPEG_ROOT/src

desktop {
    static_ffmpeg {
        LIBS += -lz -lxcb -lxcb-shm -lxcb-xfixes -lxcb-shape -lmp3lame -lbz2
        LIBS += -L$$FFMPEG_ROOT/build/amd64/ \
                -l:libavformat.a \
                -l:libavcodec.a \
                -l:libswscale.a \
                -l:libswresample.a \
                -l:libavdevice.a \
                -l:libavutil.a
    } else {
        LIBS += -lmp3lame -lavdevice -lavutil -lavformat -lavcodec -lswscale -lswresample
    }
}

sailfish {
    LIBS += -lz -lbz2
    x86 {
        LIBS += -L$$FFMPEG_ROOT/build/i486/ \
                -l:libavutil.so.56 \
                -l:libavcodec.so.58 \
                -l:libavformat.so.58 \
                -l:libavdevice.so.58 \
                -l:libswresample.so.3 \
                -l:libswscale.so.5
    }
    arm {
        LIBS += -L$$FFMPEG_ROOT/build/armv7hl/ \
                -l:libavutil.so.56 \
                -l:libavcodec.so.58 \
                -l:libavformat.so.58 \
                -l:libavdevice.so.58 \
                -l:libswresample.so.3 \
                -l:libswscale.so.5
    }

    x86: libffmpeg.files = $$FFMPEG_ROOT/build/i486/*
    arm: libffmpeg.files = $$FFMPEG_ROOT/build/armv7hl/*
    libffmpeg.path = /usr/share/$${TARGET}/lib
    INSTALLS += libffmpeg
}
