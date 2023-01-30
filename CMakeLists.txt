# Copyright (C) 2023 Michal Kosciesza <michal@mkiol.net>
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

cmake_minimum_required(VERSION 3.5)

option(with_plasma "enable plasma ui" OFF)
option(with_sfos "enable sfos ui" OFF)

option(with_flatpak "enable flatpak build" OFF)

option(with_v4l2 "enable v4l2 video sources" ON)
option(with_v4l2m2m "enable v4l2 video sources and v4l2m2m video encoder" ON)
option(with_nvenc "enable nvidia video encoder" ON)
option(with_x11_screen_capture "enable X11 screen capture video source" ON)
option(with_droidcam "enable gstreamer droidcam video source" ON)
option(with_sfos_screen_capture "enable sfos screen capture" ON)
option(with_sfos_harbour "enable sfos build for harbour" OFF)

option(with_trace_logs "enable trace logging" OFF)
option(with_sanitizers "enable asan and ubsan in debug build" ON)
option(with_static_sanitizers "link asan and ubsan statically in debug build" OFF)

option(build_x264 "download x264, build and link statically" ON)
option(build_ffmpeg "download FFmpeg, build and link statically" ON)
option(build_mhd "downlaod Libmicrohttpd, build and link statically" ON)
option(build_fmt "download fmt, build and link statically" ON)
option(build_lame "download lame, build and link statically" ON)
option(build_ffnvc "download and include ffnvc" ON)
option(build_nasm "download nasm sources and build" ON)
option(build_gumbo "download gumbo, build and link statically" ON)
option(build_npupnp "download npupnp, build and link statically" ON)
option(build_upnpp "download upnpp, build and link statically" ON)
option(build_taglib "download taglib, build and link statically" ON)
option(build_pybind11 "download pybind11, build and link statically" ON)
option(build_libarchive "download libarchive, build and link statically" ON)
option(build_pymodules "make python modules archive" ON)

if(${with_plasma} AND ${with_sfos})
    message(FATAL_ERROR "options with_plasma and with_sfos cannot be set together")
endif()

if(NOT ${with_plasma} AND NOT ${with_sfos})
    message(FATAL_ERROR "one of the options should be set (with_plasma or with_sfos)")
endif()

if(with_nvenc)
    message(INFO "option with_nvenc is supported only on x86/x86_64 architectures")
endif()

if(with_plasma)
    set(with_droidcam OFF)
    set(with_sfos_screen_capture OFF)
    set(with_sfos_harbour OFF)
    set(build_pymodules OFF)
endif()

if(with_sfos)
    set(with_v4l2 OFF)
    set(with_v4l2m2m OFF)
    set(with_nvenc OFF)
    set(with_x11_screen_capture OFF)
endif()

if(with_sfos_harbour)
    set(build_pymodules OFF)
endif()

if(with_v4l2m2m)
    set(with_v4l2 ON)
endif()

set(ffmpeg_source_url "https://ffmpeg.org/releases/ffmpeg-5.1.2.tar.gz")
set(ffmpeg_checksum "f44232183ae1ef814eac50dd382a2d7f")
set(ffnvc_source_url "https://github.com/FFmpeg/nv-codec-headers/releases/download/n11.1.5.2/nv-codec-headers-11.1.5.2.tar.gz")
set(ffnvc_checksum "94fd5479bf934595530d9bfae93c1c55")
set(mhd_source_url "https://ftpmirror.gnu.org/libmicrohttpd/libmicrohttpd-0.9.75.tar.gz")
set(mhd_checksum "aff64581937b53f3a23b05216ad2cd02")
set(fmt_source_url "https://github.com/fmtlib/fmt/releases/download/9.1.0/fmt-9.1.0.zip")
set(fmt_checksum "6133244fe8ef6f75c5601e8069b37b04")
set(x264_source_url "https://code.videolan.org/videolan/x264/-/archive/stable/x264-stable.tar.bz2")
set(x264_checksum "7bd5f01a556e643e6aaca48e3e391d50")
set(lame_source_url "https://altushost-swe.dl.sourceforge.net/project/lame/lame/3.100/lame-3.100.tar.gz")
set(lame_checksum "83e260acbe4389b54fe08e0bdbf7cddb")
set(nasm_source_url "https://www.nasm.us/pub/nasm/releasebuilds/2.15.05/nasm-2.15.05.tar.gz")
set(nams_checksum "4ab99e8e777c249f32d5c10e82c658f1")
set(gumbo_source_url "https://github.com/google/gumbo-parser/archive/refs/tags/v0.10.1.tar.gz")
set(gumbo_checksum "c6f75c9eda65e897c242f8958a34aed0")
set(taglib_source_url "https://github.com/taglib/taglib/releases/download/v1.13/taglib-1.13.tar.gz")
set(taglib_checksum "0b75619b67b9149cfcbbe5c127df6395")
set(pybind11_source_url "https://github.com/pybind/pybind11/archive/refs/tags/v2.10.3.tar.gz")
set(pybind11_checksum "a093dac9dfd613a5a7c2afa50a301098")
set(npupnp_source_url "https://www.lesbonscomptes.com/upmpdcli/downloads/libnpupnp-5.0.0.tar.gz")
set(npupnp_checksum "85bc87c7b89bc6c76c46ddcc2bd33cde")
set(upnpp_source_url "https://www.lesbonscomptes.com/upmpdcli/downloads/libupnpp-0.22.4.tar.gz")
set(upnpp_checksum "31b2bd7cb9b4d6f4306046c07960c029")
set(libarchive_source_url "https://libarchive.org/downloads/libarchive-3.6.2.tar.gz")
set(libarchive_checksum "b5b8efa8cba29396816d0dd5f61f3de3")

set(info_id "jupii")
set(info_binary_id ${info_id})
set(info_translations_id ${info_id})
if(with_flatpak)
    set(info_id "org.mkiol.Jupii")
endif()
if(with_sfos)
    set(info_binary_id "harbour-jupii")
endif()
set(info_name "Jupii")
set(info_org "org.mkiol")
set(info_author "Michal Kosciesza")
set(info_author_email "michal@mkiol.net")
set(info_copyright_year "2018-2023")
set(info_support_email "jupii@mkiol.net")
set(info_webpage "https://github.com/mkiol/Jupii")
set(info_license "Mozilla Public License 2.0")
set(info_license_url "http://mozilla.org/MPL/2.0")
set(info_license_spdx "MPL-2.0")
set(info_dbus_service "org.mkiol.jupii")
set(info_dbus_interface "org.mkiol.jupii")
set(info_translators_str "Åke Engelbrektson · Atlochowski · Carlos Gonzalez · d9h20f · Вячеслав Диконов · drosjesjaafoer · Rui Kon · Boštjan Štrumbelj · jgibbon · Fra · Petr Tsymbarovich · Carmen Fernández B.")
set(info_libs_str "Qt · QHTTPServer · npupnp · Libupnpp · TagLib · FFmpeg · LAME · x264 · Gumbo · yt-dlp · ytmusicapi · EasyEXIF · AudioTube · {fmt}")

project(${info_id} VERSION 2.14.0)

# dirs

set(external_dir ${PROJECT_SOURCE_DIR}/external)
set(external_include_dir ${PROJECT_BINARY_DIR}/external/include)
set(external_lib_dir ${PROJECT_BINARY_DIR}/external/lib)
set(plasma_dir ${PROJECT_SOURCE_DIR}/plasma)
set(sfos_dir ${PROJECT_SOURCE_DIR}/sfos)
set(resources_dir ${PROJECT_SOURCE_DIR}/resources)
set(flatpak_dir ${PROJECT_SOURCE_DIR}/flatpak)
set(translations_dir ${PROJECT_SOURCE_DIR}/translations)
set(tools_dir ${PROJECT_SOURCE_DIR}/tools)
set(patches_dir ${PROJECT_SOURCE_DIR}/patches)

# sources

set(sources
    src/albummodel.cpp
    src/albummodel.h
    src/artistmodel.cpp
    src/artistmodel.h
    src/avtransport.cpp
    src/avtransport.h
    src/bcapi.cpp
    src/bcapi.h
    src/bcmodel.cpp
    src/bcmodel.h
    src/cdirmodel.cpp
    src/cdirmodel.h
    src/connectivitydetector.cpp
    src/connectivitydetector.h
    src/contentdirectory.cpp
    src/contentdirectory.h
    src/contentserver.cpp
    src/contentserver.h
    src/contentserverworker.cpp
    src/contentserverworker.h
    src/dbusapp.cpp
    src/dbusapp.h
    src/dbus_jupii_adaptor.cpp
    src/dbus_jupii_adaptor.h
    src/dbus_notifications_inf.cpp
    src/dbus_notifications_inf.h
    src/dbus_tracker_inf.cpp
    src/dbus_tracker_inf.h
    src/device.cpp
    src/device.h
    src/deviceinfo.cpp
    src/deviceinfo.h
    src/devicemodel.cpp
    src/devicemodel.h
    src/directory.cpp
    src/directory.h
    src/dirmodel.cpp
    src/dirmodel.h
    src/dnscontentdeterminator.cpp
    src/dnscontentdeterminator.h
    src/downloader.cpp
    src/downloader.h
    src/exif.cpp
    src/exif.h
    src/filedownloader.cpp
    src/filedownloader.h
    src/filemetadata.cpp
    src/filemetadata.h
    src/fosdemmodel.cpp
    src/fosdemmodel.h
    src/frontiersiliconxc.cpp
    src/frontiersiliconxc.h
    src/gpoddermodel.cpp
    src/gpoddermodel.h
    src/gumbotools.cpp
    src/gumbotools.h
    src/icecastmodel.cpp
    src/icecastmodel.h
    src/iconprovider.cpp
    src/iconprovider.h
    src/itemmodel.cpp
    src/itemmodel.h
    src/listmodel.cpp
    src/listmodel.h
    src/main.cpp
    src/notifications.cpp
    src/notifications.h
    src/playlistfilemodel.cpp
    src/playlistfilemodel.h
    src/playlistmodel.cpp
    src/playlistmodel.h
    src/playlistparser.cpp
    src/playlistparser.h
    src/recmodel.cpp
    src/recmodel.h
    src/renderingcontrol.cpp
    src/renderingcontrol.h
    src/service.cpp
    src/service.h
    src/services.cpp
    src/services.h
    src/settings.cpp
    src/settings.h
    src/singleton.h
    src/somafmmodel.cpp
    src/somafmmodel.h
    src/soundcloudapi.cpp
    src/soundcloudapi.h
    src/soundcloudmodel.cpp
    src/soundcloudmodel.h
    src/taskexecutor.cpp
    src/taskexecutor.h
    src/thumb.cpp
    src/thumb.h
    src/tracker.cpp
    src/trackercursor.cpp
    src/trackercursor.h
    src/tracker.h
    src/trackmodel.cpp
    src/trackmodel.h
    src/transcoder.cpp
    src/transcoder.h
    src/tuneinmodel.cpp
    src/tuneinmodel.h
    src/utils.cpp
    src/utils.h
    src/xc.cpp
    src/xc.h
    src/yamahaxc.cpp
    src/yamahaxc.h
    src/ytdlapi.cpp
    src/ytdlapi.h
    src/ytmodel.cpp
    src/ytmodel.h
    src/ytmusic.cpp
    src/ytmusic.h
    src/contentdirectoryservice.cpp
    src/contentdirectoryservice.h
    src/connectionmanagerservice.cpp
    src/connectionmanagerservice.h
    src/logger.cpp
    src/logger.hpp
    src/qtlogger.cpp
    src/qtlogger.hpp
    src/avlogger.cpp
    src/avlogger.hpp
    src/databuffer.cpp
    src/databuffer.hpp
    src/caster.cpp
    src/caster.hpp
    src/fftools.cpp
    src/fftools.hpp
    src/testsource.cpp
    src/testsource.hpp
    src/radionetmodel.cpp
    src/radionetmodel.hpp
    src/radionetapi.cpp
    src/radionetapi.hpp)

if(with_plasma)
    list(APPEND sources
        ${plasma_dir}/resources.qrc
        ${CMAKE_BINARY_DIR}/translations.qrc)
endif()

if(with_sfos)
    list(APPEND sources
        src/resourcehandler.cpp
        src/resourcehandler.h)
endif()

if(with_sfos_screen_capture)
    list(APPEND sources
        src/lipstickrecordersource.cpp
        src/lipstickrecordersource.hpp
        src/wayland-lipstick-recorder-protocol.c
        src/wayland-lipstick-recorder-client-protocol.h
        src/lipstick-recorder.h)
endif()

if(with_droidcam)
    list(APPEND sources
        src/droidcamsource.cpp
        src/droidcamsource.hpp
        src/orientationmonitor.cpp
        src/orientationmonitor.hpp)
endif()

# executable

set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOMOC ON)

add_executable(${info_binary_id} ${sources})

# strip

add_custom_command(TARGET ${info_binary_id} POST_BUILD
    COMMAND $<$<CONFIG:Release>:${CMAKE_STRIP}>
    ARGS --strip-all $<TARGET_FILE:${info_binary_id}>
)

# flags and definitions

add_library(compiler_flags INTERFACE)
target_compile_features(compiler_flags INTERFACE cxx_std_17)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    if(with_sanitizers)
        target_compile_options(compiler_flags INTERFACE
            "-pie;-Wall;-Wextra;-Wpedantic;-O1;-g3;-fsanitize=address,undefined")
        if(with_static_sanitizers)
            target_link_options(compiler_flags INTERFACE
              "-pie;-static-libasan;-static-libubsan;-fsanitize=address,undefined")
        else()
            target_link_options(compiler_flags INTERFACE "-pie;-fsanitize=address,undefined")
        endif()
    else()
        target_compile_options(compiler_flags INTERFACE
            "-pie;-Wall;-Wextra;-Wpedantic;-O1;-g3")
        target_link_options(compiler_flags INTERFACE "-pie")
    endif()
else()
    target_compile_options(compiler_flags INTERFACE "-pie;-Wall;-Wextra;-Wpedantic;-O3;-g")
    target_link_options(compiler_flags INTERFACE "-pie")
endif()

target_compile_definitions(compiler_flags INTERFACE "$<$<BOOL:${with_trace_logs}>:USE_TRACE_LOGS>")
target_compile_definitions(compiler_flags INTERFACE "$<$<BOOL:${with_droidcam}>:USE_DROIDCAM>")
target_compile_definitions(compiler_flags INTERFACE "$<$<BOOL:${with_v4l2}>:USE_V4L2>")
target_compile_definitions(compiler_flags INTERFACE "$<$<BOOL:${with_v4l2m2m}>:USE_V4L2>")
target_compile_definitions(compiler_flags INTERFACE "$<$<BOOL:${with_x11_screen_capture}>:USE_X11CAPTURE>")
target_compile_definitions(compiler_flags INTERFACE "$<$<BOOL:${with_sfos}>:USE_SFOS>")
target_compile_definitions(compiler_flags INTERFACE "$<$<BOOL:${with_sfos_harbour}>:USE_SFOS_HARBOUR>")
target_compile_definitions(compiler_flags INTERFACE "$<$<BOOL:${with_sfos_screen_capture}>:USE_LIPSTICK_RECORDER>")
target_compile_definitions(compiler_flags INTERFACE "$<$<BOOL:${with_plasma}>:USE_PLASMA>")
target_compile_definitions(compiler_flags INTERFACE "$<$<CONFIG:Debug>:DEBUG>")
target_compile_definitions(compiler_flags INTERFACE "$<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:QT_QML_DEBUG>")

set_property(TARGET ${info_binary_id} PROPERTY POSITION_INDEPENDENT_CODE ON)

target_link_libraries(${info_binary_id}
    compiler_flags
    pthread
    ${CMAKE_DL_LIBS}
)

# translations

set(enabled_translations "de en es nl nl_BE pl ru sv sl_SI it zh_CN")

find_package(Qt5 COMPONENTS Core LinguistTools)

file(GLOB ts_files ${translations_dir}/*.ts)
qt5_create_translation(qm_files ${CMAKE_SOURCE_DIR}/src ${plasma_dir}/qml ${sfos_dir}/qml ${ts_files})

add_custom_command(
  OUTPUT translations.qrc
  COMMAND sh -c "${tools_dir}/make_translations_qrc.sh ${info_translations_id} /translations ${enabled_translations}" >
    "${CMAKE_BINARY_DIR}/translations.qrc"
  DEPENDS ${qm_files}
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  VERBATIM)
add_library(translations STATIC "${CMAKE_BINARY_DIR}/translations.qrc")
target_link_libraries(${info_binary_id} translations)

# config

configure_file(config.h.in config.h)
target_include_directories(${info_binary_id} PRIVATE ${PROJECT_BINARY_DIR})
target_include_directories(${info_binary_id} PRIVATE ${external_include_dir})

# dependencies

include(FindPkgConfig)
include(ExternalProject)

add_subdirectory(${external_dir}/qhttpserver)
target_include_directories(${info_binary_id} PRIVATE ${external_dir}/qhttpserver/src)
target_link_libraries(${info_binary_id} qhttpserver)

pkg_search_module(pulse REQUIRED libpulse)
target_include_directories(${info_binary_id} PRIVATE ${pulse_INCLUDE_DIRS})
target_link_libraries(${info_binary_id} ${pulse_LIBRARIES})

pkg_search_module(zlib REQUIRED zlib)
target_include_directories(${info_binary_id} PRIVATE ${zlib_INCLUDE_DIRS})
target_link_libraries(${info_binary_id} ${zlib_LIBRARIES})

pkg_search_module(curl REQUIRED libcurl)
target_include_directories(${info_binary_id} PRIVATE ${curl_INCLUDE_DIRS})
target_link_libraries(${info_binary_id} ${curl_LIBRARIES})

pkg_search_module(expat REQUIRED expat)
target_include_directories(${info_binary_id} PRIVATE ${expat_INCLUDE_DIRS})
target_link_libraries(${info_binary_id} ${expat_LIBRARIES})

find_package(Qt5 COMPONENTS Core Network Multimedia Qml Xml Sql Gui Quick DBus LinguistTools REQUIRED)
target_link_libraries(${info_binary_id} Qt5::Core Qt5::Network Qt5::Multimedia
    Qt5::Xml Qt5::Sql Qt5::Gui Qt5::Quick Qt5::DBus)

if(with_plasma)
    find_package(Qt5 COMPONENTS QuickControls2 Widgets REQUIRED)
    target_link_libraries(${info_binary_id} Qt5::QuickControls2 Qt5::Widgets)
endif()

if(with_sfos)
    pkg_search_module(sailfish REQUIRED sailfishapp)
    target_include_directories(${info_binary_id} PRIVATE ${sailfish_INCLUDE_DIRS})
    target_link_libraries(${info_binary_id} ${sailfish_LIBRARIES})

    pkg_search_module(audioresource REQUIRED audioresource)
    target_include_directories(${info_binary_id} PRIVATE ${audioresource_INCLUDE_DIRS})
    target_link_libraries(${info_binary_id} ${audioresource_LIBRARIES})

    pkg_search_module(mlite5 REQUIRED mlite5)
    target_include_directories(${info_binary_id} PRIVATE ${mlite5_INCLUDE_DIRS})
    target_link_libraries(${info_binary_id} ${mlite5_LIBRARIES})

    pkg_search_module(lzma REQUIRED liblzma)
    target_include_directories(${info_binary_id} PRIVATE ${lzma_INCLUDE_DIRS})
    target_link_libraries(${info_binary_id} ${lzma_LIBRARIES})

    pkg_search_module(keepalive REQUIRED keepalive)
    target_include_directories(${info_binary_id} PRIVATE ${keepalive_INCLUDE_DIRS})
    target_link_libraries(${info_binary_id} ${keepalive_LIBRARIES})

    if(build_libarchive)
        ExternalProject_Add(libarchive
            SOURCE_DIR ${external_dir}/libarchive
            BINARY_DIR ${PROJECT_BINARY_DIR}/external/libarchive
            INSTALL_DIR ${PROJECT_BINARY_DIR}/external
            URL "${libarchive_source_url}"
            URL_MD5 "${libarchive_checksum}"
            CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=<INSTALL_DIR> --bindir=<INSTALL_DIR>/bin
                --enable-shared=no --enable-static=yes --disable-acl --disable-xattr --disable-largefile
                --without-zstd --without-lz4 --without-libiconv-prefix --without-iconv --without-libb2
                --without-bz2lib --without-zlib --without-cng --without-openssl --without-xml2 --without-expat
                --without-lzma --with-pic=yes
            BUILD_COMMAND ${MAKE}
            BUILD_ALWAYS False
            INSTALL_COMMAND make DESTDIR=/ install)
        set(libarchive_LIBRARIES ${external_lib_dir}/libarchive.a)
        target_include_directories(${info_binary_id} PRIVATE ${external_include_dir})
        target_link_libraries(${info_binary_id} ${libarchive_LIBRARIES})
        add_dependencies(${info_binary_id} libarchive)
    else()
        pkg_search_module(libarchive REQUIRED libarchive)
        target_include_directories(${info_binary_id} PRIVATE ${libarchive_INCLUDE_DIRS})
        target_link_libraries(${info_binary_id} ${libarchive_LIBRARIES})
    endif()

    if(build_pymodules)
        add_custom_command(
          OUTPUT python.tar.xz
          COMMAND sh -c "${tools_dir}/make_pymodules.sh ${patches_dir}/yt_dlp.patch ${CMAKE_BINARY_DIR}/external/pymodules 3.8 ${PROJECT_BINARY_DIR}/python.tar.xz"
          WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
          VERBATIM)
        add_library(pymodules STATIC "${CMAKE_BINARY_DIR}/python.tar.xz")
        target_link_libraries(${info_binary_id} pymodules)
    endif()
endif()

if(with_droidcam)
    pkg_search_module(gst REQUIRED gstreamer-1.0)
    target_include_directories(${info_binary_id} PRIVATE ${gst_INCLUDE_DIRS})
    target_link_libraries(${info_binary_id} ${gst_LIBRARIES})

    pkg_search_module(gst_app REQUIRED gstreamer-app-1.0)
    target_include_directories(${info_binary_id} PRIVATE ${gst_app_INCLUDE_DIRS})
    target_link_libraries(${info_binary_id} ${gst_app_LIBRARIES})
endif()

if (with_sfos_screen_capture)
    pkg_search_module(wayland REQUIRED wayland-client)
    target_include_directories(${info_binary_id} PRIVATE ${wayland_INCLUDE_DIRS} ${Qt5Gui_PRIVATE_INCLUDE_DIRS})
    target_link_libraries(${info_binary_id} ${wayland_LIBRARIES})
endif()

if (with_x11_screen_capture)
    pkg_search_module(xcb REQUIRED xcb)
    include_directories(${xcb_INCLUDE_DIRS})
    target_link_libraries(${info_binary_id} ${xcb_LIBRARIES})

    pkg_search_module(x11 REQUIRED x11)
    include_directories(${x11_INCLUDE_DIRS})
    target_link_libraries(${info_binary_id} ${x11_LIBRARIES})
endif()

if(build_ffmpeg)
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
        --enable-muxer=mp4
        --enable-muxer=mpegts
        --enable-muxer=mp3
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
        --enable-bsf=h264_mp4toannexb
        --enable-bsf=dump_extradata
        --enable-bsf=extract_extradata
        --enable-libx264
        --enable-libmp3lame)

    if(${with_v4l2} OR ${with_v4l2m2m})
        list(APPEND ffmpeg_opts
            --enable-indev=v4l2)
    endif()

    if(with_v4l2m2m)
        list(APPEND ffmpeg_opts
            --enable-encoder=h264_v4l2m2m
            --enable-v4l2_m2m)
    endif()

    if(with_nvenc)
        list(APPEND ffmpeg_opts
            --enable-encoder=h264_nvenc
            --enable-nvenc
            --enable-ffnvcodec)
    endif()

    if(with_x11_screen_capture)
        list(APPEND ffmpeg_opts
            --enable-indev=xcbgrab
            --enable-libxcb)
    endif()

    ExternalProject_Add(ffmpeg
        SOURCE_DIR ${external_dir}/ffmpeg
        BINARY_DIR ${PROJECT_BINARY_DIR}/external/ffmpeg
        INSTALL_DIR ${PROJECT_BINARY_DIR}/external
        URL "${ffmpeg_source_url}"
        PATCH_COMMAND patch --forward --unified ${external_dir}/ffmpeg/configure
            -i ${patches_dir}/ffmpeg_v4l2.patch ||
                echo "patch cmd failed, likely already patched"
        CONFIGURE_COMMAND CPATH=${external_include_dir}
            LIBRARY_PATH=${external_lib_dir}
            PATH=$ENV{PATH}:${PROJECT_BINARY_DIR}/external/bin PKG_CONFIG_PATH=${external_lib_dir}/pkgconfig
            <SOURCE_DIR>/configure --prefix=<INSTALL_DIR> ${ffmpeg_opts} --extra-ldflags=-L${external_lib_dir}
        BUILD_COMMAND CPATH=${external_include_dir}
            LIBRARY_PATH=${external_lib_dir}
            PATH=$ENV{PATH}:${PROJECT_BINARY_DIR}/external/bin ${MAKE}
        BUILD_ALWAYS False
        INSTALL_COMMAND CPATH=${external_include_dir}
            LIBRARY_PATH=${external_lib_dir}
            PATH=$ENV{PATH}:${PROJECT_BINARY_DIR}/external/bin make DESTDIR=/ install)
    find_library(libm_path m)
    target_include_directories(${info_binary_id} PRIVATE ${external_include_dir})
    set(ffmpeg_LIBRARIES
        ${PROJECT_BINARY_DIR}/external/lib/libavfilter.a
        ${PROJECT_BINARY_DIR}/external/lib/libavdevice.a
        ${PROJECT_BINARY_DIR}/external/lib/libavformat.a
        ${PROJECT_BINARY_DIR}/external/lib/libavcodec.a
        ${PROJECT_BINARY_DIR}/external/lib/libswresample.a
        ${PROJECT_BINARY_DIR}/external/lib/libswscale.a
        ${PROJECT_BINARY_DIR}/external/lib/libavutil.a
        ${libm_path})
    target_link_libraries(${info_binary_id} ${ffmpeg_LIBRARIES})
    add_dependencies(${info_binary_id} ffmpeg)
else()
    pkg_search_module(ffmpeg_avutil REQUIRED libavutil>=57.17.100)
    pkg_search_module(ffmpeg_avcodec REQUIRED libavcodec>=59.18.100)
    pkg_search_module(ffmpeg_avformat REQUIRED libavformat>=59.16.100)
    pkg_search_module(ffmpeg_avdevice REQUIRED libavdevice>=59.7.100)
    pkg_search_module(ffmpeg_avfilter REQUIRED libavfilter>=8.44.100)
    pkg_search_module(ffmpeg_swresample REQUIRED libswresample>=4.3.100)
    pkg_search_module(ffmpeg_swscale REQUIRED libswscale>=6.7.100)
    target_include_directories(${info_binary_id} PRIVATE
        ${ffmpeg_avutil_INCLUDE_DIRS}
        ${ffmpeg_avcoodec_INCLUDE_DIRS}
        ${ffmpeg_avformat_INCLUDE_DIRS}
        ${ffmpeg_avdevice_INCLUDE_DIRS}
        ${ffmpeg_avfilter_INCLUDE_DIRS}
        ${ffmpeg_swresample_INCLUDE_DIRS}
        ${ffmpeg_swscale_INCLUDE_DIRS})
    target_link_libraries(${info_binary_id}
        ${ffmpeg_avdevice_LIBRARIES}
        ${ffmpeg_avfilter_LIBRARIES}
        ${ffmpeg_avformat_LIBRARIES}
        ${ffmpeg_avcoodec_LIBRARIES}
        ${ffmpeg_swresample_LIBRARIES}
        ${ffmpeg_swscale_LIBRARIES}
        ${ffmpeg_avutil_LIBRARIES})
endif()

if(build_x264)
    ExternalProject_Add(x264
        SOURCE_DIR ${external_dir}/x264
        BINARY_DIR ${PROJECT_BINARY_DIR}/external/x264
        INSTALL_DIR ${PROJECT_BINARY_DIR}/external
        URL "${x264_source_url}"
        URL_MD5 "${x264_checksum}"
        CONFIGURE_COMMAND PATH=$ENV{PATH}:${PROJECT_BINARY_DIR}/external/bin <SOURCE_DIR>/configure --prefix=<INSTALL_DIR> --enable-pic --enable-static --disable-cli
        BUILD_COMMAND PATH=$ENV{PATH}:${PROJECT_BINARY_DIR}/external/bin ${MAKE}
        BUILD_ALWAYS False
        INSTALL_COMMAND PATH=$ENV{PATH}:${PROJECT_BINARY_DIR}/external/bin make DESTDIR=/ install)
    ExternalProject_Add_StepDependencies(ffmpeg configure x264)
    set(x264_LIBRARIES ${external_lib_dir}/libx264.a)
    target_include_directories(${info_binary_id} PRIVATE ${external_include_dir})
    target_link_libraries(${info_binary_id} ${x264_LIBRARIES} ${CMAKE_DL_LIBS})
    add_dependencies(${info_binary_id} x264)
else()
    pkg_search_module(x264 REQUIRED x264)
    target_include_directories(${info_binary_id} PRIVATE ${x264_INCLUDE_DIRS})
    target_link_libraries(${info_binary_id} ${x264_LIBRARIES})
endif()

if(with_nvenc)
    if (build_ffnvc)
        ExternalProject_Add(ffnvc
            SOURCE_DIR ${external_dir}/ffnvc
            BINARY_DIR ${PROJECT_BINARY_DIR}/external/ffnvc
            INSTALL_DIR ${PROJECT_BINARY_DIR}/external
            URL "${ffnvc_source_url}"
            URL_MD5 "${ffnvc_checksum}"
            CONFIGURE_COMMAND cp -r --no-target-directory <SOURCE_DIR> <BINARY_DIR>
            BUILD_COMMAND ""
            BUILD_ALWAYS False
            INSTALL_COMMAND make DESTDIR=/ PREFIX=<INSTALL_DIR> install)
        ExternalProject_Add_StepDependencies(ffmpeg configure ffnvc)
        target_include_directories(${info_binary_id} PRIVATE ${external_include_dir})
    endif()
endif()

if(build_lame)
    ExternalProject_Add(lame
        SOURCE_DIR ${external_dir}/lame
        BINARY_DIR ${PROJECT_BINARY_DIR}/external/lame
        INSTALL_DIR ${PROJECT_BINARY_DIR}/external
        URL "${lame_source_url}"
        URL_MD5 "${lame_checksum}"
        CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=<INSTALL_DIR> --bindir=<INSTALL_DIR>/bin
            --enable-static --enable-nasm --disable-decoder --disable-analyzer-hooks
            --disable-frontend --with-pic=yes
        BUILD_COMMAND ${MAKE}
        BUILD_ALWAYS False
        INSTALL_COMMAND make DESTDIR=/ install)
    ExternalProject_Add_StepDependencies(ffmpeg configure lame)
    set(lame_LIBRARIES ${external_lib_dir}/libmp3lame.a)
    target_include_directories(${info_binary_id} PRIVATE ${external_include_dir})
    target_link_libraries(${info_binary_id} ${lame_LIBRARIES})
    add_dependencies(${info_binary_id} lame)
else()
    find_library(lame_path mp3lame)
    target_link_libraries(${info_binary_id} ${lame_path})
endif()

if(build_gumbo)
    ExternalProject_Add(gumbo
        SOURCE_DIR ${external_dir}/gumbo
        BINARY_DIR ${PROJECT_BINARY_DIR}/external/gumbo
        INSTALL_DIR ${PROJECT_BINARY_DIR}/external
        URL "${gumbo_source_url}"
        URL_MD5 "${gumbo_checksum}"
        CONFIGURE_COMMAND sh -c "cd <SOURCE_DIR> && <SOURCE_DIR>/autogen.sh" &&
            <SOURCE_DIR>/configure --prefix=<INSTALL_DIR>
            --enable-static --disable-shared --with-pic=yes
        BUILD_COMMAND ${MAKE}
        BUILD_ALWAYS False
        INSTALL_COMMAND make DESTDIR=/ install)
    set(gumbo_LIBRARIES ${external_lib_dir}/libgumbo.a)
    target_include_directories(${info_binary_id} PRIVATE ${external_include_dir})
    target_link_libraries(${info_binary_id} ${gumbo_LIBRARIES})
    add_dependencies(${info_binary_id} gumbo)
else()
    pkg_search_module(gumbo REQUIRED gumbo)
    target_include_directories(${info_binary_id} PRIVATE ${gumbo_INCLUDE_DIRS})
    target_link_libraries(${info_binary_id} ${gumbo_LIBRARIES})
endif()

if(build_upnpp)
    ExternalProject_Add(upnpp
        SOURCE_DIR ${external_dir}/upnpp
        BINARY_DIR ${PROJECT_BINARY_DIR}/external/upnpp
        INSTALL_DIR ${PROJECT_BINARY_DIR}/external
        URL "${upnpp_source_url}"
        URL_MD5 "${upnpp_checksum}"
        CONFIGURE_COMMAND PKG_CONFIG_PATH=${PROJECT_BINARY_DIR}/external/lib/pkgconfig
            <SOURCE_DIR>/configure --prefix=<INSTALL_DIR> --enable-static --disable-shared --with-pic=yes
        BUILD_COMMAND ${MAKE}
        BUILD_ALWAYS False
        INSTALL_COMMAND make DESTDIR=/ install)
    set(upnpp_LIBRARIES ${external_lib_dir}/libupnpp.a ${external_lib_dir}/libupnpputil.a)
    target_include_directories(${info_binary_id} PRIVATE ${external_include_dir})
    target_link_libraries(${info_binary_id} ${upnpp_LIBRARIES})
    add_dependencies(${info_binary_id} upnpp)
else()
    pkg_search_module(upnpp REQUIRED libupnpp)
    target_include_directories(${info_binary_id} PRIVATE ${upnpp_INCLUDE_DIRS})
    target_link_libraries(${info_binary_id} ${upnpp_LIBRARIES} upnpputil)
endif()

if(build_npupnp)
    ExternalProject_Add(npupnp
        SOURCE_DIR ${external_dir}/npupnp
        BINARY_DIR ${PROJECT_BINARY_DIR}/external/npupnp
        INSTALL_DIR ${PROJECT_BINARY_DIR}/external
        URL "${npupnp_source_url}"
        URL_MD5 "${npupnp_checksum}"
        CONFIGURE_COMMAND PKG_CONFIG_PATH=${PROJECT_BINARY_DIR}/external/lib/pkgconfig
            <SOURCE_DIR>/configure --prefix=<INSTALL_DIR> --enable-static --disable-shared --with-pic=yes
        BUILD_COMMAND ${MAKE}
        BUILD_ALWAYS False
        INSTALL_COMMAND make DESTDIR=/ install)
    ExternalProject_Add_StepDependencies(upnpp configure npupnp)
    set(npupnp_LIBRARIES ${external_lib_dir}/libnpupnp.a)
    target_include_directories(${info_binary_id} PRIVATE ${external_include_dir})
    target_link_libraries(${info_binary_id} ${npupnp_LIBRARIES})
    add_dependencies(${info_binary_id} npupnp)
else()
    pkg_search_module(npupnp REQUIRED libnpupnp)
    target_include_directories(${info_binary_id} PRIVATE ${npupnp_INCLUDE_DIRS})
    target_link_libraries(${info_binary_id} ${npupnp_LIBRARIES})
endif()

if(build_mhd)
    ExternalProject_Add(mhd
        SOURCE_DIR ${external_dir}/mhd
        BINARY_DIR ${PROJECT_BINARY_DIR}/external/mhd
        INSTALL_DIR ${PROJECT_BINARY_DIR}/external
        URL "${mhd_source_url}"
        URL_MD5 "${mhd_checksum}"
        CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=<INSTALL_DIR>
        --disable-doc --disable-examples --disable-curl --disable-https
        --disable-postprocessor --disable-dauth --disable-bauth --disable-epoll
        --disable-sendfile --disable-httpupgrade
        --enable-static --disable-shared --with-pic=yes
        BUILD_COMMAND ${MAKE}
        BUILD_ALWAYS False
        INSTALL_COMMAND make DESTDIR=/ install)
    ExternalProject_Add_StepDependencies(npupnp configure mhd)
    set(mhd_LIBRARIES ${external_lib_dir}/libmicrohttpd.a)
    target_include_directories(${info_binary_id} PRIVATE ${external_include_dir})
    target_link_libraries(${info_binary_id} ${mhd_LIBRARIES})
    add_dependencies(${info_binary_id} mhd)
else()
    pkg_search_module(mhd REQUIRED libmicrohttpd)
    target_include_directories(${info_binary_id} PRIVATE ${mhd_INCLUDE_DIRS})
    target_link_libraries(${info_binary_id} ${mhd_LIBRARIES})
endif()

if(build_fmt)
    ExternalProject_Add(fmt
        SOURCE_DIR ${external_dir}/fmt
        BINARY_DIR ${PROJECT_BINARY_DIR}/external/fmt
        INSTALL_DIR ${PROJECT_BINARY_DIR}/external
        URL "${fmt_source_url}"
        URL_MD5 "${fmt_checksum}"
        CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=/ -DCMAKE_INSTALL_LIBDIR=/lib
            -DCMAKE_INSTALL_INCLUDEDIR=/include -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DFMT_DOC=OFF
            -DFMT_TEST=OFF -DFMT_FUZZ=OFF
        BUILD_ALWAYS False
        INSTALL_COMMAND make DESTDIR=${PROJECT_BINARY_DIR}/external install/local)
    set(fmt_LIBRARIES ${external_lib_dir}/libfmt.a)
    target_include_directories(${info_binary_id} PRIVATE ${external_include_dir})
    target_link_libraries(${info_binary_id} ${fmt_LIBRARIES})
    add_dependencies(${info_binary_id} fmt)
else()
    pkg_search_module(fmt REQUIRED fmt)
    target_include_directories(${info_binary_id} PRIVATE ${fmt_INCLUDE_DIRS})
    target_link_libraries(${info_binary_id} ${fmt_LIBRARIES})
endif()

if(build_taglib)
    ExternalProject_Add(taglib
        SOURCE_DIR ${external_dir}/taglib
        BINARY_DIR ${PROJECT_BINARY_DIR}/external/taglib
        INSTALL_DIR ${PROJECT_BINARY_DIR}/external
        URL "${taglib_source_url}"
        URL_MD5 "${taglib_checksum}"
        CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=/ -DCMAKE_INSTALL_LIBDIR=/lib
            -DCMAKE_INSTALL_INCLUDEDIR=/include -DCMAKE_POSITION_INDEPENDENT_CODE=ON
            -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF -DBUILD_BINDINGS=OFF -DBUILD_SHARED_LIBS=OFF
            -DENABLE_STATIC_RUNTIME=OFF
        BUILD_ALWAYS False
        INSTALL_COMMAND make DESTDIR=${PROJECT_BINARY_DIR}/external install)
    set(taglib_LIBRARIES ${external_lib_dir}/libtag.a)
    target_include_directories(${info_binary_id} PRIVATE ${external_include_dir})
    target_link_libraries(${info_binary_id} ${taglib_LIBRARIES})
    add_dependencies(${info_binary_id} taglib)
else()
    pkg_search_module(taglib REQUIRED taglib)
    target_include_directories(${info_binary_id} PRIVATE ${taglib_INCLUDE_DIRS})
    target_link_libraries(${info_binary_id} ${taglib_LIBRARIES})
endif()

if(build_nasm)
    ExternalProject_Add(nasm
        SOURCE_DIR ${external_dir}/nasm
        BINARY_DIR ${PROJECT_BINARY_DIR}/external/nasm
        INSTALL_DIR ${PROJECT_BINARY_DIR}/external
        URL "${nasm_source_url}"
        URL_MD5 "${nasm_checksum}"
        CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=<INSTALL_DIR>
        BUILD_COMMAND ${MAKE}
        BUILD_ALWAYS False
        INSTALL_COMMAND make DESTDIR=/ install)
    ExternalProject_Add_StepDependencies(ffmpeg configure nasm)
    ExternalProject_Add_StepDependencies(x264 configure nasm)
    if(${build_lame} AND ${with_mp3_format})
        ExternalProject_Add_StepDependencies(lame configure nasm)
    endif()
endif()

if(NOT ${with_sfos_harbour})
    find_package(Python COMPONENTS Development REQUIRED)
    target_include_directories(${info_binary_id} PRIVATE ${Python_INCLUDE_DIRS})
    target_link_directories(${info_binary_id} PRIVATE ${Python_LIBRARY_DIRS})
    target_link_libraries(${info_binary_id} ${Python_LIBRARIES})

    if(build_pybind11)
        ExternalProject_Add(pybind11
            SOURCE_DIR ${external_dir}/pybind11
            BINARY_DIR ${PROJECT_BINARY_DIR}/external/pybind11
            INSTALL_DIR ${PROJECT_BINARY_DIR}/external
            URL "${pybind11_source_url}"
            URL_MD5 "${pybind11_checksum}"
            CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=/ -DCMAKE_INSTALL_LIBDIR=/lib
                -DCMAKE_INSTALL_INCLUDEDIR=/include -DCMAKE_POSITION_INDEPENDENT_CODE=ON
                -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF -DPYBIND11_INSTALL=ON
                -DPYBIND11_TEST=OFF -DPYBIND11_FINDPYTHON=ON
            BUILD_ALWAYS False
            INSTALL_COMMAND make DESTDIR=${PROJECT_BINARY_DIR}/external install)
        set(pybind11_LIBRARIES ${external_lib_dir}/libtag.a)
        target_include_directories(${info_binary_id} PRIVATE ${external_include_dir})
        target_link_libraries(${info_binary_id} ${pybind11_LIBRARIES})
        add_dependencies(${info_binary_id} pybind11)
    else()
        pkg_search_module(pybind11 REQUIRED pybind11)
        target_include_directories(${info_binary_id} PRIVATE ${pybind11_INCLUDE_DIRS})
        target_link_libraries(${info_binary_id} ${pybind11_LIBRARIES})
    endif()
endif()

# install

install(TARGETS ${info_binary_id} RUNTIME DESTINATION bin)

if(with_sfos)
    if (with_sfos_harbour)
        install(FILES ${sfos_dir}/${info_binary_id}-harbour.desktop DESTINATION share/applications RENAME ${info_binary_id}.desktop)
    else()
        install(FILES ${sfos_dir}/${info_binary_id}.desktop DESTINATION share/applications)
        install(FILES ${sfos_dir}/${info_binary_id}-open-url.desktop DESTINATION share/applications)
        install(FILES ${sfos_dir}/sailjail/Jupii.permission DESTINATION /etc/sailjail/permissions)
        install(FILES ${sfos_dir}/sailjail/JupiiGpodder.permission DESTINATION /etc/sailjail/permissions)
    endif()

    if(build_pymodules)
        install(FILES ${PROJECT_BINARY_DIR}/python.tar.xz DESTINATION share/${info_binary_id}/lib)
    endif()

    install(FILES ${sfos_dir}/icons/86x86/${info_binary_id}.png DESTINATION share/icons/hicolor/86x86/apps)
    install(FILES ${sfos_dir}/icons/108x108/${info_binary_id}.png DESTINATION share/icons/hicolor/108x108/apps)
    install(FILES ${sfos_dir}/icons/128x128/${info_binary_id}.png DESTINATION share/icons/hicolor/128x128/apps)
    install(FILES ${sfos_dir}/icons/172x172/${info_binary_id}.png DESTINATION share/icons/hicolor/172x172/apps)

    install(FILES ${qm_files} DESTINATION share/${info_binary_id}/translations)

    install(DIRECTORY ${sfos_dir}/qml DESTINATION share/${info_binary_id})
    install(DIRECTORY ${sfos_dir}/images DESTINATION share/${info_binary_id})
endif()

if(with_plasma)
    install(FILES ${plasma_dir}/${info_binary_id}.desktop DESTINATION share/applications)
    install(FILES ${plasma_dir}/icons/32x32/${info_binary_id}.png DESTINATION share/icons/hicolor/32x32/apps)
    install(FILES ${plasma_dir}/icons/48x48/${info_binary_id}.png DESTINATION share/icons/hicolor/48x48/apps)
    install(FILES ${plasma_dir}/icons/64x64/${info_binary_id}.png DESTINATION share/icons/hicolor/64x64/apps)
    install(FILES ${plasma_dir}/icons/72x72/${info_binary_id}.png DESTINATION share/icons/hicolor/72x72/apps)
    install(FILES ${plasma_dir}/icons/96x96/${info_binary_id}.png DESTINATION share/icons/hicolor/96x96/apps)
    install(FILES ${plasma_dir}/icons/128x128/${info_binary_id}.png DESTINATION share/icons/hicolor/128x128/apps)
    install(FILES ${plasma_dir}/icons/150x150/${info_binary_id}.png DESTINATION share/icons/hicolor/150x150/apps)
    install(FILES ${plasma_dir}/icons/192x192/${info_binary_id}.png DESTINATION share/icons/hicolor/192x192/apps)
    install(FILES ${plasma_dir}/icons/256x256/${info_binary_id}.png DESTINATION share/icons/hicolor/256x256/apps)
    install(FILES ${plasma_dir}/icons/512x512/${info_binary_id}.png DESTINATION share/icons/hicolor/512x512/apps)
endif()

# misc

if(with_sfos)
    file(WRITE "${CMAKE_BINARY_DIR}/QtCreatorDeployment.txt"
        "${CMAKE_INSTALL_PREFIX}\n${CMAKE_BINARY_DIR}/${info_binary_id}:bin\n")
endif()
