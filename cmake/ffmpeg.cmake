set(ffmpeg_source_url "https://ffmpeg.org/releases/ffmpeg-7.1.tar.xz")
set(ffmpeg_checksum "40973d44970dbc83ef302b0609f2e74982be2d85916dd2ee7472d30678a7abe6")

set(lame_source_url "https://altushost-swe.dl.sourceforge.net/project/lame/lame/3.100/lame-3.100.tar.gz")
set(lame_checksum "ddfe36cab873794038ae2c1210557ad34857a4b6bdc515785d1da9e175b1da1e")

set(nasm_source_url "https://www.nasm.us/pub/nasm/releasebuilds/2.15.05/nasm-2.15.05.tar.gz")
set(nasm_checksum "9182a118244b058651c576baa9d0366ee05983c4d4ae1d9ddd3236a9f2304997")

set(ffnvc_source_url "https://github.com/FFmpeg/nv-codec-headers/releases/download/n12.1.14.0/nv-codec-headers-12.1.14.0.tar.gz")
set(ffnvc_checksum "62b30ab37e4e9be0d0c5b37b8fee4b094e38e570984d56e1135a6b6c2c164c9f")

set(x264_source_url "https://code.videolan.org/videolan/x264.git")
set(x264_tag "31e19f92f00c7003fa115047ce50978bc98c3a0d")

ExternalProject_Add(nasm
    SOURCE_DIR ${external_dir}/nasm
    BINARY_DIR ${PROJECT_BINARY_DIR}/external/nasm
    INSTALL_DIR ${PROJECT_BINARY_DIR}/external
    URL "${nasm_source_url}"
    URL_HASH SHA256=${nasm_checksum}
    CONFIGURE_COMMAND cp -r --no-target-directory <SOURCE_DIR> <BINARY_DIR> &&
        <BINARY_DIR>/configure --prefix=<INSTALL_DIR>
    BUILD_COMMAND ${MAKE}
    BUILD_ALWAYS False
    INSTALL_COMMAND make DESTDIR=/ install
)

ExternalProject_Add(lame
    SOURCE_DIR ${external_dir}/lame
    BINARY_DIR ${PROJECT_BINARY_DIR}/external/lame
    INSTALL_DIR ${PROJECT_BINARY_DIR}/external
    URL "${lame_source_url}"
    URL_HASH SHA256=${lame_checksum}
    CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=<INSTALL_DIR>
        --bindir=<INSTALL_DIR>/bin --libdir=<INSTALL_DIR>/lib
        --enable-static=true --enable-shared=false
        --enable-nasm --disable-decoder --disable-analyzer-hooks
        --disable-frontend --with-pic=yes
    BUILD_COMMAND ${MAKE}
    BUILD_ALWAYS False
    INSTALL_COMMAND make DESTDIR=/ install
)

ExternalProject_Add(x264
    SOURCE_DIR ${external_dir}/x264
    BINARY_DIR ${PROJECT_BINARY_DIR}/external/x264
    INSTALL_DIR ${PROJECT_BINARY_DIR}/external
    GIT_REPOSITORY ${x264_source_url}
    GIT_TAG ${x264_tag}
    UPDATE_COMMAND ""
    CONFIGURE_COMMAND PATH=$ENV{PATH}:${external_bin_dir}
        PKG_CONFIG_PATH=${external_lib_dir}/pkgconfig
        <SOURCE_DIR>/configure --prefix=<INSTALL_DIR> --enable-pic --enable-static --disable-cli
    BUILD_COMMAND PATH=$ENV{PATH}:${external_bin_dir}n ${MAKE}
    BUILD_ALWAYS False
    INSTALL_COMMAND PATH=$ENV{PATH}:${external_bin_dir} make DESTDIR=/ install)

set(ffmpeg_opts
    --disable-autodetect
    --disable-doc
    --disable-programs
    --disable-everything
    --enable-static
    --disable-shared
    --enable-nonfree
    --enable-gpl
    --enable-pic
    --enable-protocol=file
    --enable-filter=vflip
    --enable-filter=hflip
    --enable-filter=scale
    --enable-filter=transpose
    --enable-filter=color
    --enable-filter=overlay
    --enable-filter=pad
    --enable-filter=volume
    --enable-filter=amix
    --enable-filter=dynaudnorm
    --enable-filter=aresample
    --enable-filter=aformat
    --enable-encoder=libx264
    --enable-encoder=aac
    --enable-encoder=libmp3lame
    --enable-decoder=h264
    --enable-decoder=rawvideo
    --enable-decoder=pcm_u8
    --enable-decoder=pcm_u32le
    --enable-decoder=pcm_u32be
    --enable-decoder=pcm_u24le
    --enable-decoder=pcm_u24be
    --enable-decoder=pcm_u16le
    --enable-decoder=pcm_u16be
    --enable-decoder=pcm_s8
    --enable-decoder=pcm_s32le
    --enable-decoder=pcm_s32be
    --enable-decoder=pcm_s24le
    --enable-decoder=pcm_s24be
    --enable-decoder=pcm_s16le
    --enable-decoder=pcm_s16be
    --enable-decoder=pcm_f64le
    --enable-decoder=pcm_f64be
    --enable-decoder=pcm_f32le
    --enable-decoder=pcm_f32be
    --enable-decoder=aac
    --enable-decoder=aac_fixed
    --enable-decoder=aac_latm
    --enable-decoder=mp3
    --enable-decoder=mp3adu
    --enable-decoder=mp3adufloat
    --enable-decoder=mp3float
    --enable-decoder=mp3on4
    --enable-decoder=mp3on4float
    --enable-decoder=flac
    --enable-muxer=mp4
    --enable-muxer=mpegts
    --enable-muxer=mp3
    --enable-muxer=flac
    --enable-demuxer=mpegts
    --enable-demuxer=h264
    --enable-demuxer=rawvideo
    --enable-demuxer=aac
    --enable-demuxer=mp3
    --enable-demuxer=mov
    --enable-demuxer=ogg
    --enable-demuxer=matroska
    --enable-demuxer=flac
    --enable-demuxer=wav
    --enable-demuxer=mpegvideo
    --enable-parser=h264
    --enable-parser=aac
    --enable-parser=aac_latm
    --enable-parser=ac3
    --enable-bsf=h264_mp4toannexb
    --enable-bsf=dump_extradata
    --enable-bsf=extract_extradata
    --enable-libx264
    --enable-libmp3lame)

if(${WITH_V4L2} OR ${WITH_V4L2M2M})
    list(APPEND ffmpeg_opts
        --enable-indev=v4l2)
endif()

if(WITH_V4L22M2)
    list(APPEND ffmpeg_opts
        --enable-encoder=h264_v4l2m2m
        --enable-v4l2_m2m)
endif()

if(WITH_NVENC)
    list(APPEND ffmpeg_opts
        --enable-encoder=h264_nvenc
        --enable-nvenc
        --enable-ffnvcodec)
endif()

if(WITH_X11_SCREEN_CAPTURE)
    list(APPEND ffmpeg_opts
        --enable-indev=xcbgrab
        --enable-libxcb)
endif()

set(ffmpeg_extra_ldflags -L${external_lib_dir})
set(ffmpeg_extra_libs "-lm")

ExternalProject_Add(ffmpeg
    SOURCE_DIR ${external_dir}/ffmpeg
    BINARY_DIR ${PROJECT_BINARY_DIR}/external/ffmpeg
    INSTALL_DIR ${PROJECT_BINARY_DIR}/external
    URL ${ffmpeg_source_url}
    URL_HASH SHA256=${ffmpeg_checksum}
    CONFIGURE_COMMAND CPATH=${external_include_dir}
        LIBRARY_PATH=${external_lib_dir}
        PATH=$ENV{PATH}:${external_bin_dir} PKG_CONFIG_PATH=${external_lib_dir}/pkgconfig
        <SOURCE_DIR>/configure --prefix=<INSTALL_DIR> ${ffmpeg_opts}
        --extra-ldflags=${ffmpeg_extra_ldflags}
        --extra-libs=${ffmpeg_extra_libs}
    BUILD_COMMAND CPATH=${external_include_dir}
        LIBRARY_PATH=${external_lib_dir}
        PATH=$ENV{PATH}:${external_bin_dir} ${MAKE}
    BUILD_ALWAYS False
    INSTALL_COMMAND CPATH=${external_include_dir}
        LIBRARY_PATH=${external_lib_dir}
        PATH=$ENV{PATH}:${external_bin_dir} make DESTDIR=/ install
)

ExternalProject_Add_StepDependencies(lame configure nasm)
ExternalProject_Add_StepDependencies(x264 configure nasm)
ExternalProject_Add_StepDependencies(ffmpeg configure nasm)
ExternalProject_Add_StepDependencies(ffmpeg configure lame)
ExternalProject_Add_StepDependencies(ffmpeg configure x264)

if(WITH_NVENC)
    ExternalProject_Add(ffnvc
        SOURCE_DIR ${external_dir}/ffnvc
        BINARY_DIR ${PROJECT_BINARY_DIR}/external/ffnvc
        INSTALL_DIR ${PROJECT_BINARY_DIR}/external
        URL ${ffnvc_source_url}
        URL_HASH SHA256=${ffnvc_checksum}
        CONFIGURE_COMMAND cp -r --no-target-directory <SOURCE_DIR> <BINARY_DIR>
        BUILD_COMMAND ""
        BUILD_ALWAYS False
        INSTALL_COMMAND make DESTDIR=/ PREFIX=<INSTALL_DIR> install)
    ExternalProject_Add_StepDependencies(ffmpeg configure ffnvc)
endif()

list(APPEND deps_libs
    ${external_lib_dir}/libavfilter.a
    ${external_lib_dir}/libavdevice.a
    ${external_lib_dir}/libavformat.a
    ${external_lib_dir}/libavcodec.a
    ${external_lib_dir}/libswresample.a
    ${external_lib_dir}/libswscale.a
    ${external_lib_dir}/libavutil.a
    ${external_lib_dir}/libmp3lame.a
    ${external_lib_dir}/libx264.a)
list(APPEND deps ffmpeg lame x264)

