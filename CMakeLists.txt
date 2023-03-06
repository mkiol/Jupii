# Copyright (C) 2023 Michal Kosciesza <michal@mkiol.net>
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

cmake_minimum_required(VERSION 3.5)

option(with_plasma "enable plasma ui" OFF)
option(with_sfos "enable sfos ui" OFF)

option(with_flatpak "enable flatpak build" OFF)
option(with_tests "enable tests" OFF)

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
option(build_catch2 "download catch2, build and link statically" ON)

if(${with_plasma} AND ${with_sfos})
    message(FATAL_ERROR "options with_plasma and with_sfos cannot be set together")
endif()

if(NOT ${with_plasma} AND NOT ${with_sfos})
    message(FATAL_ERROR "one of the options should be set (with_plasma or with_sfos)")
endif()

if(with_nvenc)
    message(STATUS "option with_nvenc is supported only on x86/x86_64 architectures")
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
set(catch2_source_url "https://github.com/catchorg/Catch2/archive/refs/tags/v3.3.2.tar.gz")
set(catch2_checksum "6a15aa49dbdee13f279b73cab16ae203")

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

project(${info_id} VERSION 2.14.2)

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
set(tests_dir ${PROJECT_SOURCE_DIR}/tests)
set(sources_dir ${PROJECT_SOURCE_DIR}/src)

# global config

set(BUILD_SHARED_LIBS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOMOC ON)

# includes

set(includes ${sources_dir} ${PROJECT_BINARY_DIR} ${external_include_dir})

# jupii_lib

set(jupii_lib_sources
    ${sources_dir}/albummodel.cpp
    ${sources_dir}/albummodel.h
    ${sources_dir}/artistmodel.cpp
    ${sources_dir}/artistmodel.h
    ${sources_dir}/avtransport.cpp
    ${sources_dir}/avtransport.h
    ${sources_dir}/bcapi.cpp
    ${sources_dir}/bcapi.h
    ${sources_dir}/bcmodel.cpp
    ${sources_dir}/bcmodel.h
    ${sources_dir}/cdirmodel.cpp
    ${sources_dir}/cdirmodel.h
    ${sources_dir}/connectivitydetector.cpp
    ${sources_dir}/connectivitydetector.h
    ${sources_dir}/contentdirectory.cpp
    ${sources_dir}/contentdirectory.h
    ${sources_dir}/contentserver.cpp
    ${sources_dir}/contentserver.h
    ${sources_dir}/contentserverworker.cpp
    ${sources_dir}/contentserverworker.h
    ${sources_dir}/dbusapp.cpp
    ${sources_dir}/dbusapp.h
    ${sources_dir}/dbus_jupii_adaptor.cpp
    ${sources_dir}/dbus_jupii_adaptor.h
    ${sources_dir}/dbus_notifications_inf.cpp
    ${sources_dir}/dbus_notifications_inf.h
    ${sources_dir}/dbus_tracker_inf.cpp
    ${sources_dir}/dbus_tracker_inf.h
    ${sources_dir}/device.cpp
    ${sources_dir}/device.h
    ${sources_dir}/deviceinfo.cpp
    ${sources_dir}/deviceinfo.h
    ${sources_dir}/devicemodel.cpp
    ${sources_dir}/devicemodel.h
    ${sources_dir}/directory.cpp
    ${sources_dir}/directory.h
    ${sources_dir}/dirmodel.cpp
    ${sources_dir}/dirmodel.h
    ${sources_dir}/dnscontentdeterminator.cpp
    ${sources_dir}/dnscontentdeterminator.h
    ${sources_dir}/downloader.cpp
    ${sources_dir}/downloader.h
    ${sources_dir}/exif.cpp
    ${sources_dir}/exif.h
    ${sources_dir}/filedownloader.cpp
    ${sources_dir}/filedownloader.h
    ${sources_dir}/filemetadata.cpp
    ${sources_dir}/filemetadata.h
    ${sources_dir}/fosdemmodel.cpp
    ${sources_dir}/fosdemmodel.h
    ${sources_dir}/frontiersiliconxc.cpp
    ${sources_dir}/frontiersiliconxc.h
    ${sources_dir}/gpoddermodel.cpp
    ${sources_dir}/gpoddermodel.h
    ${sources_dir}/gumbotools.cpp
    ${sources_dir}/gumbotools.h
    ${sources_dir}/icecastmodel.cpp
    ${sources_dir}/icecastmodel.h
    ${sources_dir}/iconprovider.cpp
    ${sources_dir}/iconprovider.h
    ${sources_dir}/itemmodel.cpp
    ${sources_dir}/itemmodel.h
    ${sources_dir}/listmodel.cpp
    ${sources_dir}/listmodel.h
    ${sources_dir}/notifications.cpp
    ${sources_dir}/notifications.h
    ${sources_dir}/playlistfilemodel.cpp
    ${sources_dir}/playlistfilemodel.h
    ${sources_dir}/playlistmodel.cpp
    ${sources_dir}/playlistmodel.h
    ${sources_dir}/playlistparser.cpp
    ${sources_dir}/playlistparser.h
    ${sources_dir}/recmodel.cpp
    ${sources_dir}/recmodel.h
    ${sources_dir}/renderingcontrol.cpp
    ${sources_dir}/renderingcontrol.h
    ${sources_dir}/service.cpp
    ${sources_dir}/service.h
    ${sources_dir}/services.cpp
    ${sources_dir}/services.h
    ${sources_dir}/settings.cpp
    ${sources_dir}/settings.h
    ${sources_dir}/singleton.h
    ${sources_dir}/somafmmodel.cpp
    ${sources_dir}/somafmmodel.h
    ${sources_dir}/soundcloudapi.cpp
    ${sources_dir}/soundcloudapi.h
    ${sources_dir}/soundcloudmodel.cpp
    ${sources_dir}/soundcloudmodel.h
    ${sources_dir}/taskexecutor.cpp
    ${sources_dir}/taskexecutor.h
    ${sources_dir}/thumb.cpp
    ${sources_dir}/thumb.h
    ${sources_dir}/tracker.cpp
    ${sources_dir}/trackercursor.cpp
    ${sources_dir}/trackercursor.h
    ${sources_dir}/tracker.h
    ${sources_dir}/trackmodel.cpp
    ${sources_dir}/trackmodel.h
    ${sources_dir}/transcoder.cpp
    ${sources_dir}/transcoder.h
    ${sources_dir}/tuneinmodel.cpp
    ${sources_dir}/tuneinmodel.h
    ${sources_dir}/utils.cpp
    ${sources_dir}/utils.h
    ${sources_dir}/xc.cpp
    ${sources_dir}/xc.h
    ${sources_dir}/yamahaxc.cpp
    ${sources_dir}/yamahaxc.h
    ${sources_dir}/ytdlapi.cpp
    ${sources_dir}/ytdlapi.h
    ${sources_dir}/ytmodel.cpp
    ${sources_dir}/ytmodel.h
    ${sources_dir}/ytmusic.cpp
    ${sources_dir}/ytmusic.h
    ${sources_dir}/contentdirectoryservice.cpp
    ${sources_dir}/contentdirectoryservice.h
    ${sources_dir}/connectionmanagerservice.cpp
    ${sources_dir}/connectionmanagerservice.h
    ${sources_dir}/logger.cpp
    ${sources_dir}/logger.hpp
    ${sources_dir}/qtlogger.cpp
    ${sources_dir}/qtlogger.hpp
    ${sources_dir}/avlogger.cpp
    ${sources_dir}/avlogger.hpp
    ${sources_dir}/databuffer.cpp
    ${sources_dir}/databuffer.hpp
    ${sources_dir}/caster.cpp
    ${sources_dir}/caster.hpp
    ${sources_dir}/fftools.cpp
    ${sources_dir}/fftools.hpp
    ${sources_dir}/testsource.cpp
    ${sources_dir}/testsource.hpp
    ${sources_dir}/radionetmodel.cpp
    ${sources_dir}/radionetmodel.hpp
    ${sources_dir}/radionetapi.cpp
    ${sources_dir}/radionetapi.hpp
    ${sources_dir}/mpdtools.cpp
    ${sources_dir}/mpdtools.hpp)

if(with_sfos)
    list(APPEND jupii_lib_sources
        src/resourcehandler.cpp
        src/resourcehandler.h)
endif()

if(with_sfos_screen_capture)
    list(APPEND jupii_lib_sources
        src/lipstickrecordersource.cpp
        src/lipstickrecordersource.hpp
        src/wayland-lipstick-recorder-protocol.c
        src/wayland-lipstick-recorder-client-protocol.h
        src/lipstick-recorder.h)
endif()

if(with_droidcam)
    list(APPEND jupii_lib_sources
        src/droidcamsource.cpp
        src/droidcamsource.hpp
        src/orientationmonitor.cpp
        src/orientationmonitor.hpp)
endif()

configure_file(config.h.in config.h)

add_library(jupii_lib STATIC ${jupii_lib_sources})

# resources

set(resources "")
if(with_plasma)
    list(APPEND resources
        ${plasma_dir}/resources.qrc
        ${CMAKE_BINARY_DIR}/translations.qrc)
endif()

# executable

add_executable(${info_binary_id} ${resources} "${sources_dir}/main.cpp")
target_link_libraries(${info_binary_id} jupii_lib)

add_custom_command(TARGET ${info_binary_id} POST_BUILD
    COMMAND $<$<CONFIG:Release>:${CMAKE_STRIP}>
    ARGS --strip-all $<TARGET_FILE:${info_binary_id}>
)

# tests

if(with_tests)
    if(build_catch2)
        include(FetchContent)

        FetchContent_Declare(
          Catch2
          URL ${catch2_source_url}
          URL_HASH MD5=${catch2_checksum}
          SOURCE_DIR "${external_dir}/catch2"
          BINARY_DIR "${PROJECT_BINARY_DIR}/external/catch2"
          INSTALL_DIR "${PROJECT_BINARY_DIR}/external"
        )
        FetchContent_MakeAvailable(Catch2)
    else()
        find_package(Catch2 3 REQUIRED)
    endif()

    file(GLOB tests_src
        "${tests_dir}/*.hpp"
        "${tests_dir}/*.cpp"
    )

    add_executable(tests ${tests_src})
    target_link_libraries(tests Catch2::Catch2WithMain)
    target_link_libraries(tests jupii_lib)

    list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)

    include(CTest)
    include(Catch)

    catch_discover_tests(tests)
endif()

# flags and definitions

if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo")
endif ()

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

# translations

set(enabled_translations de en es nl nl_BE pl ru sv sl_SI it zh_CN)

find_package(Qt5 COMPONENTS Core LinguistTools)

set(ts_files "")
foreach(lang ${enabled_translations})
    list(APPEND ts_files "${translations_dir}/jupii-${lang}.ts")
endforeach()

qt5_create_translation(qm_files ${CMAKE_SOURCE_DIR}/src ${plasma_dir}/qml ${sfos_dir}/qml ${ts_files})

string(REPLACE ";" " " enabled_translations_str "${enabled_translations}")
add_custom_command(
  OUTPUT translations.qrc
  COMMAND sh -c "${tools_dir}/make_translations_qrc.sh ${info_translations_id} /translations ${CMAKE_BINARY_DIR}/translations.qrc ${enabled_translations_str}"
  DEPENDS ${qm_files}
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  VERBATIM)
add_library(translations STATIC "${CMAKE_BINARY_DIR}/translations.qrc")
target_link_libraries(${info_binary_id} translations)

# dependencies

set(deps "")
set(deps_libs compiler_flags pthread ${CMAKE_DL_LIBS})
set(deps_dirs "")

include(FindPkgConfig)
include(ExternalProject)

add_subdirectory(${external_dir}/qhttpserver)
list(APPEND deps_libs qhttpserver)
list(APPEND includes ${external_dir}/qhttpserver/src)

pkg_search_module(pulse REQUIRED libpulse)
list(APPEND deps_libs ${pulse_LIBRARIES})
list(APPEND includes ${pulse_INCLUDE_DIRS})

pkg_search_module(zlib REQUIRED zlib)
list(APPEND deps_libs ${zlib_LIBRARIES})
list(APPEND includes ${zlib_INCLUDE_DIRS})

find_package(Qt5 COMPONENTS Core Network Multimedia Qml Xml Sql Gui Quick DBus LinguistTools REQUIRED)
list(APPEND deps_libs Qt5::Core Qt5::Network Qt5::Multimedia
    Qt5::Xml Qt5::Sql Qt5::Gui Qt5::Quick Qt5::DBus)

if(with_plasma)
    find_package(Qt5 COMPONENTS QuickControls2 Widgets REQUIRED)
    list(APPEND deps_libs Qt5::QuickControls2 Qt5::Widgets)
endif()

if(with_sfos)
    pkg_search_module(sailfish REQUIRED sailfishapp)
    list(APPEND deps_libs ${sailfish_LIBRARIES})
    list(APPEND includes ${sailfish_INCLUDE_DIRS})

    pkg_search_module(audioresource REQUIRED audioresource)
    list(APPEND deps_libs ${audioresource_LIBRARIES})
    list(APPEND includes ${audioresource_INCLUDE_DIRS})

    pkg_search_module(mlite5 REQUIRED mlite5)
    list(APPEND deps_libs ${mlite5_LIBRARIES})
    list(APPEND includes ${mlite5_INCLUDE_DIRS})

    pkg_search_module(lzma REQUIRED liblzma)
    list(APPEND deps_libs ${lzma_LIBRARIES})
    list(APPEND includes ${lzma_INCLUDE_DIRS})

    pkg_search_module(keepalive REQUIRED keepalive)
    list(APPEND deps_libs ${keepalive_LIBRARIES})
    list(APPEND includes ${keepalive_INCLUDE_DIRS})

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
        list(APPEND deps libarchive)
        list(APPEND deps_libs ${external_lib_dir}/libarchive.a)
    else()
        pkg_search_module(libarchive REQUIRED libarchive)
        list(APPEND deps_libs ${libarchive_LIBRARIES})
        list(APPEND includes ${libarchive_INCLUDE_DIRS})
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
    pkg_search_module(glib REQUIRED glib-2.0)
    list(APPEND deps_libs ${glib_LIBRARIES})
    list(APPEND includes ${glib_INCLUDE_DIRS})

    pkg_search_module(gst REQUIRED gstreamer-1.0)
    list(APPEND deps_libs ${gst_LIBRARIES})
    list(APPEND includes ${gst_INCLUDE_DIRS})

    pkg_search_module(gst_app REQUIRED gstreamer-app-1.0)
    list(APPEND deps_libs ${gst_app_LIBRARIES})
    list(APPEND includes ${gst_app_INCLUDE_DIRS})
endif()

if (with_sfos_screen_capture)
    pkg_search_module(wayland REQUIRED wayland-client)
    list(APPEND deps_libs ${wayland_LIBRARIES})
    list(APPEND includes ${wayland_INCLUDE_DIRS} ${Qt5Gui_PRIVATE_INCLUDE_DIRS})
endif()

if (with_x11_screen_capture)
    pkg_search_module(xcb REQUIRED xcb)
    list(APPEND deps_libs ${xcb_LIBRARIES})
    list(APPEND includes ${xcb_INCLUDE_DIRS})

    pkg_search_module(x11 REQUIRED x11)
    list(APPEND deps_libs ${x11_LIBRARIES})
    list(APPEND includes ${x11_INCLUDE_DIRS})
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
        --enable-parser=aac
        --enable-parser=aac_latm
        --enable-parser=ac3
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
    list(APPEND deps_libs
        m
        ${PROJECT_BINARY_DIR}/external/lib/libavfilter.a
        ${PROJECT_BINARY_DIR}/external/lib/libavdevice.a
        ${PROJECT_BINARY_DIR}/external/lib/libavformat.a
        ${PROJECT_BINARY_DIR}/external/lib/libavcodec.a
        ${PROJECT_BINARY_DIR}/external/lib/libswresample.a
        ${PROJECT_BINARY_DIR}/external/lib/libswscale.a
        ${PROJECT_BINARY_DIR}/external/lib/libavutil.a)
    list(APPEND deps ffmpeg)
else()
    pkg_search_module(ffmpeg_avutil REQUIRED libavutil>=57.17.100)
    pkg_search_module(ffmpeg_avcodec REQUIRED libavcodec>=59.18.100)
    pkg_search_module(ffmpeg_avformat REQUIRED libavformat>=59.16.100)
    pkg_search_module(ffmpeg_avdevice REQUIRED libavdevice>=59.7.100)
    pkg_search_module(ffmpeg_avfilter REQUIRED libavfilter>=8.44.100)
    pkg_search_module(ffmpeg_swresample REQUIRED libswresample>=4.3.100)
    pkg_search_module(ffmpeg_swscale REQUIRED libswscale>=6.7.100)
    list(APPEND deps_libs
        ${ffmpeg_avdevice_LIBRARIES}
        ${ffmpeg_avfilter_LIBRARIES}
        ${ffmpeg_avformat_LIBRARIES}
        ${ffmpeg_avcoodec_LIBRARIES}
        ${ffmpeg_swresample_LIBRARIES}
        ${ffmpeg_swscale_LIBRARIES}
        ${ffmpeg_avutil_LIBRARIES})
    list(APPEND includes
        ${ffmpeg_avutil_INCLUDE_DIRS}
        ${ffmpeg_avcoodec_INCLUDE_DIRS}
        ${ffmpeg_avformat_INCLUDE_DIRS}
        ${ffmpeg_avdevice_INCLUDE_DIRS}
        ${ffmpeg_avfilter_INCLUDE_DIRS}
        ${ffmpeg_swresample_INCLUDE_DIRS}
        ${ffmpeg_swscale_INCLUDE_DIRS})
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
    list(APPEND deps_libs ${x264_LIBRARIES} ${external_lib_dir}/libx264.a)
    list(APPEND deps x264)
else()
    pkg_search_module(x264 REQUIRED x264)
    list(APPEND deps_libs ${x264_LIBRARIES})
    list(APPEND includes ${x264_INCLUDE_DIRS})
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
    list(APPEND deps_libs ${external_lib_dir}/libmp3lame.a)
    list(APPEND deps lame)
else()
    find_library(lame_path mp3lame)
    list(APPEND deps_libs ${lame_path})
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
    list(APPEND deps_libs ${external_lib_dir}/libgumbo.a)
    list(APPEND deps gumbo)
else()
    pkg_search_module(gumbo REQUIRED gumbo)
    list(APPEND deps_libs ${gumbo_LIBRARIES})
    list(APPEND includes ${gumbo_INCLUDE_DIRS})
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
    list(APPEND deps_libs ${external_lib_dir}/libupnpp.a ${external_lib_dir}/libupnpputil.a)
    list(APPEND deps upnpp)
else()
    pkg_search_module(upnpp REQUIRED libupnpp)
    list(APPEND deps_libs ${upnpp_LIBRARIES} upnpputil)
    list(APPEND includes ${upnpp_INCLUDE_DIRS})
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
    list(APPEND deps_libs ${external_lib_dir}/libnpupnp.a)
    list(APPEND deps npupnp)
else()
    pkg_search_module(npupnp REQUIRED libnpupnp)
    list(APPEND deps_libs ${npupnp_LIBRARIES})
    list(APPEND includes ${npupnp_INCLUDE_DIRS})
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
    list(APPEND deps_libs ${external_lib_dir}/libmicrohttpd.a)
    list(APPEND deps mhd)
else()
    pkg_search_module(mhd REQUIRED libmicrohttpd)
    list(APPEND deps_libs ${mhd_LIBRARIES})
    list(APPEND includes ${mhd_INCLUDE_DIRS})
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
    list(APPEND deps_libs ${external_lib_dir}/libfmt.a)
    list(APPEND deps fmt)
else()
    pkg_search_module(fmt REQUIRED fmt)
    list(APPEND deps_libs ${fmt_LIBRARIES})
    list(APPEND includes ${fmt_INCLUDE_DIRS})
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
    list(APPEND deps_libs ${external_lib_dir}/libtag.a)
    list(APPEND deps taglib)
else()
    pkg_search_module(taglib REQUIRED taglib)
    list(APPEND deps_libs ${taglib_LIBRARIES})
    list(APPEND includes ${taglib_INCLUDE_DIRS})
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
    if(build_lame)
        ExternalProject_Add_StepDependencies(lame configure nasm)
    endif()
endif()

if(NOT ${with_sfos_harbour})
    find_package(Python COMPONENTS Development REQUIRED)
    list(APPEND deps_dirs ${Python_LIBRARY_DIRS})
    list(APPEND deps_libs ${Python_LIBRARIES})
    list(APPEND includes ${Python_INCLUDE_DIRS})

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
        list(APPEND deps_libs ${external_lib_dir}/libtag.a)
        list(APPEND deps pybind11)
    else()
        pkg_search_module(pybind11 REQUIRED pybind11)
        list(APPEND deps_libs ${pybind11_LIBRARIES})
        list(APPEND includes ${pybind11_INCLUDE_DIRS})
    endif()
endif()

pkg_search_module(curl REQUIRED libcurl)
list(APPEND deps_libs ${curl_LIBRARIES})
list(APPEND includes ${curl_INCLUDE_DIRS})

pkg_search_module(expat REQUIRED expat)
list(APPEND deps_libs ${expat_LIBRARIES})
list(APPEND includes ${expat_INCLUDE_DIRS})

target_include_directories(${info_binary_id} PRIVATE ${includes})
target_link_libraries(${info_binary_id} ${deps_libs})
if(deps)
    add_dependencies(${info_binary_id} ${deps})
endif()

target_include_directories(jupii_lib PRIVATE ${includes})
target_link_libraries(jupii_lib ${deps_libs})
target_link_directories(jupii_lib PRIVATE ${deps_dirs})
if(deps)
    add_dependencies(jupii_lib ${deps})
endif()

if(with_tests)
    target_include_directories(tests PRIVATE ${includes})
    target_link_libraries(tests ${deps_libs})
    if(deps)
        add_dependencies(tests ${deps})
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
