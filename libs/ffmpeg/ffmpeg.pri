#
# SFOS:
#
# wget https://ffmpeg.org/releases/ffmpeg-4.1.3.tar.bz2
# tar -xf ffmpeg-4.1.3.tar.bz2
# cd ffmpeg-4.1.3
# chmod u+x ./configure
# sb2 -t SailfishOS-[target] ./configure --disable-programs --disable-doc --disable-everything --enable-pic --enable-protocol=file --enable-encoder=libx264 --enable-encoder=aac --enable-decoder=rawvideo --enable-muxer=mp4 --enable-parser=h264 --disable-x86asm --enable-nonfree --enable-encoder=libx264rgb --enable-rpath --enable-gpl --enable-libx264 --enable-muxer=mpegts --enable-demuxer=aac --enable-demuxer=avi --enable-demuxer=h264 --enable-demuxer=m4v --enable-demuxer=mov --enable-demuxer=ogg --enable-demuxer=mpegvideo --enable-demuxer=matroska  --enable-demuxer=wav --enable-decoder=pcm_u8 --enable-decoder=pcm_u32le --enable-decoder=pcm_u32be --enable-decoder=pcm_u24le --enable-decoder=pcm_u24be --enable-decoder=pcm_u16le --enable-decoder=pcm_u16be --enable-decoder=pcm_s8 --enable-decoder=pcm_s32le --enable-decoder=pcm_s32be --enable-decoder=pcm_s24le --enable-decoder=pcm_s24be --enable-decoder=pcm_s16le --enable-decoder=pcm_s16be --enable-decoder=pcm_f64le   --enable-decoder=pcm_f64be --enable-decoder=pcm_f32le --enable-decoder=pcm_f32be  --enable-demuxer=pcm_u32be --enable-demuxer=pcm_u32le --enable-demuxer=pcm_u8 --enable-demuxer=pcm_alaw --enable-demuxer=pcm_f32be --enable-demuxer=pcm_f32le --enable-demuxer=pcm_f64be --enable-demuxer=pcm_f64le --enable-demuxer=pcm_s16be --enable-demuxer=pcm_s16le --enable-demuxer=pcm_s24be --enable-demuxer=pcm_s24le  --enable-demuxer=pcm_s32be --enable-demuxer=pcm_s32le --enable-demuxer=pcm_s8 --enable-demuxer=pcm_u16be --enable-demuxer=pcm_u16le --enable-demuxer=pcm_u24be --enable-demuxer=pcm_u24le --enable-libmp3lame --enable-encoder=libmp3lame --enable-muxer=mp3 --disable-static --enable-shared --disable-debug
# sb2 -t SailfishOS-[target] make
# cp -L libavcodec/libavcodec.so.58 [dest]
# cp -L libavformat/libavformat.so.58 [dest]
# cp -L libavdevice/libavdevice.so.58 [dest]
# cp -L libavutil/libavutil.so.56 [dest]
# cp -L libswresample/libswresample.so.3 [dest]
# cp -L libswscale/libswscale.so.5 [dest]
#
# Desktop
#
# ./configure --disable-programs --disable-doc --disable-everything --enable-pic --enable-protocol=file --enable-encoder=libx264 --enable-encoder=aac --enable-decoder=rawvideo --enable-muxer=mp4 --enable-parser=h264 --disable-x86asm --enable-nonfree --enable-encoder=libx264rgb --enable-rpath --enable-gpl --enable-libx264 --enable-muxer=mpegts --enable-demuxer=aac --enable-demuxer=avi --enable-demuxer=h264 --enable-demuxer=m4v --enable-demuxer=mov --enable-demuxer=ogg --enable-demuxer=mpegvideo --enable-demuxer=matroska  --enable-demuxer=wav --enable-decoder=pcm_u8 --enable-decoder=pcm_u32le --enable-decoder=pcm_u32be --enable-decoder=pcm_u24le --enable-decoder=pcm_u24be --enable-decoder=pcm_u16le --enable-decoder=pcm_u16be --enable-decoder=pcm_s8 --enable-decoder=pcm_s32le --enable-decoder=pcm_s32be --enable-decoder=pcm_s24le --enable-decoder=pcm_s24be --enable-decoder=pcm_s16le --enable-decoder=pcm_s16be --enable-decoder=pcm_f64le   --enable-decoder=pcm_f64be --enable-decoder=pcm_f32le --enable-decoder=pcm_f32be  --enable-demuxer=pcm_u32be --enable-demuxer=pcm_u32le --enable-demuxer=pcm_u8 --enable-demuxer=pcm_alaw --enable-demuxer=pcm_f32be --enable-demuxer=pcm_f32le --enable-demuxer=pcm_f64be --enable-demuxer=pcm_f64le --enable-demuxer=pcm_s16be --enable-demuxer=pcm_s16le --enable-demuxer=pcm_s24be --enable-demuxer=pcm_s24le  --enable-demuxer=pcm_s32be --enable-demuxer=pcm_s32le --enable-demuxer=pcm_s8 --enable-demuxer=pcm_u16be --enable-demuxer=pcm_u16le --enable-demuxer=pcm_u24be --enable-demuxer=pcm_u24le --enable-libmp3lame --enable-encoder=libmp3lame --enable-muxer=mp3 --disable-shared --enable-static --disable-debug --enable-indev=xcbgrab --enable-cuda --enable-nvenc --enable-encoder=h264_nvenc --enable-encoder=h264_vaapi
#

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

    x86 {
        LIBS += -L$$FFMPEG_ROOT/build/i486 \
                -l:libavutil.so.56 \
                -l:libavcodec.so.58 \
                -l:libavformat.so.58 \
                -l:libavdevice.so.58 \
                -l:libswresample.so.3 \
                -l:libswscale.so.5
        libffmpeg.files = $$FFMPEG_ROOT/build/i486/*
    }

    arm {
        LIBS += -L$$FFMPEG_ROOT/build/armv7hl \
                -l:libavutil.so.56 \
                -l:libavcodec.so.58 \
                -l:libavformat.so.58 \
                -l:libavdevice.so.58 \
                -l:libswresample.so.3 \
                -l:libswscale.so.5
        libffmpeg.files = $$FFMPEG_ROOT/build/armv7hl/*
    }

    libffmpeg.path = /usr/share/$${TARGET}/lib
    INSTALLS += libffmpeg
}

desktop {
    PKGCONFIG += zlib bzip2 xcb xcb-shm xcb-xfixes xcb-shape x11 libva \
                 libva-drm libva-x11 vdpau
    LIBS += -L$$FFMPEG_ROOT/build/amd64 \
            -l:libswscale.a \
            -l:libavdevice.a \
            -l:libavformat.a \
            -l:libavfilter.a \
            -l:libavcodec.a \
            -l:libavutil.a \
            -l:libswresample.a
}
