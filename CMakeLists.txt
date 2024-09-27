# Copyright (C) 2023-2024 Michal Kosciesza <michal@mkiol.net>
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

cmake_minimum_required(VERSION 3.5)

option(WITH_DESKTOP "enable desktop ui" OFF)
option(WITH_SFOS "enable sfos ui" OFF)

option(WITH_FLATPAK "enable flatpak build" OFF)
option(WITH_TESTS "enable tests" OFF)

option(WITH_V4L2 "enable v4l2 video sources" ON)
option(WITH_V4L2M2M "enable v4l2 video sources and v4l2m2m video encoder" ON)
option(WITH_NVENC "enable nvidia video encoder" ON)
option(WITH_X11_SCREEN_CAPTURE "enable X11 screen capture video source" ON)
option(WITH_DROIDCAM "enable gstreamer droidcam video source" ON)
option(WITH_SFOS_SCREEN_CAPTURE "enable sfos screen capture" ON)
option(WITH_SFOS_HARBOUR "enable sfos build for harbour" OFF)
option(WITH_PY "enable python libraries" ON)

option(WITH_TRACE_LOGS "enable trace logging" OFF)
option(WITH_SANITIZERS "enable asan and ubsan in debug build" ON)
option(WITH_STATIC_SANITIZERS "link asan and ubsan statically in debug build" OFF)

option(BUILD_FFMPEG "download sources of ffmpeg libs, build and link statically" ON)
option(BUILD_FMT "download sources of fmt, build and link statically" ON)
option(BUILD_GUMBO "download gumbo, build and link statically" ON)
option(BUILD_UPNPP "download sources of upnpp, build and link statically" ON)
option(BUILD_TAGLIB "download sources of taglib, build and link statically" ON)
option(BUILD_PYBIND11 "download sources of pybind11, build and link statically" ON)
option(BUILD_LIBARCHIVE "download sources of libarchive, build and link statically" ON)
option(BUILD_XZ "download sources of xz lib, build and link statically" ON)
option(BUILD_PYTHON_MODULE "make python module archive" ${WITH_SFOS})
option(BUILD_CATCH2 "download sources of catch2, build and link statically" ON)

if(${WITH_DESKTOP} AND ${WITH_SFOS})
    message(FATAL_ERROR "options WITH_DESKTOP and WITH_SFOS cannot be set together")
endif()

if(NOT ${WITH_DESKTOP} AND NOT ${WITH_SFOS})
    message(FATAL_ERROR "one of the options should be set (WITH_DESKTOP or WITH_SFOS)")
endif()

if(WITH_DESKTOP)
    set(WITH_DROIDCAM OFF)
    set(WITH_SFOS_SCREEN_CAPTURE OFF)
    set(WITH_SFOS_HARBOUR OFF)
    if(WITH_FLATPAK)
        set(BUILD_PYTHON_MODULE OFF)
    endif()
endif()

if(WITH_SFOS)
    set(WITH_V4L2 OFF)
    set(WITH_V4L2M2M OFF)
    set(WITH_NVENC OFF)
    set(WITH_X11_SCREEN_CAPTURE OFF)
endif()

if(WITH_SFOS_HARBOUR)
    set(BUILD_PYTHON_MODULE OFF)
endif()

if(WITH_V4L2M2M)
    set(WITH_V4L2 ON)
endif()

set(id "jupii")
set(info_name "Jupii")
set(info_org "net.mkiol")
set(info_domain "mkiol.net")
set(info_dbus_app_service "net.mkiol.jupii")
set(info_dbus_app_path "/net/mkiol/jupii")
set(info_dbus_app_id ${id})
set(info_id ${id})
set(info_binary_id ${id})
set(info_translations_id ${id})
set(info_app_icon_id ${id})
if(WITH_SFOS)
    set(info_org "org.mkiol")
    set(info_domain "mkiol.org")
    set(info_dbus_app_service "org.mkiol.jupii")
    set(info_dbus_app_path "/org/mkiol/jupii")
    set(info_binary_id "harbour-jupii")
    set(info_app_icon_id "harbour-jupii")
endif()
if(WITH_FLATPAK)
    set(info_dbus_app_service "net.mkiol.Jupii")
    set(info_dbus_app_path "/net/mkiol/Jupii")
    set(info_dbus_app_id "Jupii")
    set(info_app_icon_id "net.mkiol.Jupii")
endif()
set(info_author "Michal Kosciesza")
set(info_author_email "michal@mkiol.net")
set(info_copyright_year "2018-2024")
set(info_support_email "${id}@mkiol.net")
set(info_webpage "https://github.com/mkiol/${id}")
set(info_license "Mozilla Public License 2.0")
set(info_license_url "http://mozilla.org/MPL/2.0")
set(info_license_spdx "MPL-2.0")
set(info_translators_str "Åke Engelbrektson · Atlochowski · Carlos Gonzalez · d9h20f · \
Вячеслав Диконов · drosjesjaafoer · Rui Kon · Boštjan Štrumbelj · jgibbon · Fra · \
Petr Tsymbarovich · Carmen Fernández B. · Priit Jõerüüt")
set(info_libs_str "Qt · QHTTPServer · npupnp · Libupnpp · TagLib · FFmpeg · LAME · \
x264 · Gumbo · yt-dlp · ytmusicapi · EasyEXIF · AudioTube · {fmt}")

project(${id} VERSION 2.14.4)

if(CMAKE_SYSTEM_PROCESSOR MATCHES ".*arm.*")
    message(STATUS "arm architecture detected")
    set(arch_arm32 ON)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES ".*aarch64.*")
    message(STATUS "arm64 architecture detected")
    set(arch_arm64 ON)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES ".*86_64.*")
    message(STATUS "x86_64 architecture detected")
    set(arch_x8664 ON)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES ".*i486.*")
    message(STATUS "x86_32 architecture detected")
    set(arch_x8632 ON)
else()
    message(FATAL_ERROR "unsupported architecture: ${CMAKE_SYSTEM_PROCESSOR}")
endif()

if(${WITH_NVENC} AND NOT ${arch_x8664})
    message(STATUS "option with_nvenc is supported only on x86_64 architectures")
    set(WITH_NVENC OFF)
endif()

# dirs

include(GNUInstallDirs)

set(external_dir "${PROJECT_SOURCE_DIR}/external")
set(external_include_dir "${PROJECT_BINARY_DIR}/external/include")
set(external_lib_dir "${PROJECT_BINARY_DIR}/external/lib")
set(external_share_dir "${PROJECT_BINARY_DIR}/external/share")
set(external_bin_dir "${PROJECT_BINARY_DIR}/external/bin")
set(desktop_dir "${PROJECT_SOURCE_DIR}/desktop")
set(sfos_dir "${PROJECT_SOURCE_DIR}/sfos")
set(resources_dir "${PROJECT_SOURCE_DIR}/resources")
set(flatpak_dir "${PROJECT_SOURCE_DIR}/flatpak")
set(translations_dir "${PROJECT_SOURCE_DIR}/translations")
set(tools_dir "${PROJECT_SOURCE_DIR}/tools")
set(patches_dir "${PROJECT_SOURCE_DIR}/patches")
set(tests_dir "${PROJECT_SOURCE_DIR}/tests")
set(sources_dir "${PROJECT_SOURCE_DIR}/src")
set(systemd_dir "${PROJECT_SOURCE_DIR}/systemd")
set(dbus_dir "${PROJECT_SOURCE_DIR}/dbus")
set(binary_path "${CMAKE_INSTALL_PREFIX}/bin/${info_binary_id}")
set(cmake_path "${PROJECT_SOURCE_DIR}/cmake")
set(install_dir "share/${info_binary_id}")
set(module_install_dir ${install_dir})
set(share_install_dir "${install_dir}/share")
set(lib_install_dir "${install_dir}/lib")
set(bin_install_dir "${install_dir}/bin")
set(rpath_install_dir "${CMAKE_INSTALL_PREFIX}/${lib_install_dir}")

set(xz_path "xz")
if(WITH_FLATPAK)
    set(xz_path "/app/bin/xz")
    set(rpath_install_dir "${CMAKE_INSTALL_PREFIX}/lib")
elseif(BUILD_XZ)
    set(xz_path "${external_bin_dir}/xz")
endif()

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
    ${sources_dir}/mpdtools.hpp
    ${sources_dir}/py_executor.cpp
    ${sources_dir}/py_executor.hpp
    ${sources_dir}/py_tools.cpp
    ${sources_dir}/py_tools.hpp
    ${sources_dir}/module_tools.cpp
    ${sources_dir}/module_tools.hpp
    ${sources_dir}/checksum_tools.cpp
    ${sources_dir}/checksum_tools.hpp
    ${sources_dir}/comp_tools.cpp
    ${sources_dir}/comp_tools.hpp)

if(WITH_SFOS)
    list(APPEND jupii_lib_sources
        ${sources_dir}/resourcehandler.cpp
        ${sources_dir}/resourcehandler.h)
endif()

if(WITH_SFOS_SCREEN_CAPTURE)
    list(APPEND jupii_lib_sources
        ${sources_dir}/lipstickrecordersource.cpp
        ${sources_dir}/lipstickrecordersource.hpp
        ${sources_dir}/wayland-lipstick-recorder-protocol.c
        ${sources_dir}/wayland-lipstick-recorder-client-protocol.h
        ${sources_dir}/lipstick-recorder.h)
endif()

if(WITH_DROIDCAM)
    list(APPEND jupii_lib_sources
        ${sources_dir}/droidcamsource.cpp
        ${sources_dir}/droidcamsource.hpp
        ${sources_dir}/orientationmonitor.cpp
        ${sources_dir}/orientationmonitor.hpp)
endif()

configure_file(config.h.in config.h)

add_library(jupii_lib STATIC ${jupii_lib_sources})

# resources

set(resources "")

if(WITH_DESKTOP)
    list(APPEND resources
        ${desktop_dir}/resources.qrc
        ${CMAKE_BINARY_DIR}/translations.qrc)
endif()

# executable

add_executable(${info_binary_id} ${resources} "${sources_dir}/main.cpp")
set_property(TARGET ${info_binary_id} PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
target_link_libraries(${info_binary_id} jupii_lib)

add_custom_command(TARGET ${info_binary_id} POST_BUILD
    COMMAND $<$<CONFIG:Release>:${CMAKE_STRIP}>
    ARGS --strip-all $<TARGET_FILE:${info_binary_id}>
)

# tests

if(WITH_TESTS)
    include(${cmake_path}/tests.cmake)
endif()

# flags and definitions

if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo")
endif ()

add_library(compiler_flags INTERFACE)
target_compile_features(compiler_flags INTERFACE cxx_std_17)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    if(WITH_SANITIZERS)
        target_compile_options(compiler_flags INTERFACE
            "-pie;-Wall;-Wextra;-Wpedantic;-O1;-g3;-fsanitize=address,undefined")
        if(WITH_STATIC_SANITIZERS)
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
    target_link_options(compiler_flags INTERFACE "-pie;-fno-stack-protector")
endif()

set(link_opts "-Wl,--disable-new-dtags;-Wl,-pie;-Wl,-rpath,${rpath_install_dir}")
if(NOT ${WITH_SFOS})
    set(link_opts "${link_opts};-Wl,-rpath,${CMAKE_INSTALL_PREFIX}/lib")
endif()
target_link_options(compiler_flags INTERFACE ${link_opts})

target_compile_definitions(compiler_flags INTERFACE "$<$<BOOL:${WITH_TRACE_LOGS}>:USE_TRACE_LOGS>")
target_compile_definitions(compiler_flags INTERFACE "$<$<BOOL:${WITH_DROIDCAM}>:USE_DROIDCAM>")
target_compile_definitions(compiler_flags INTERFACE "$<$<BOOL:${WITH_V4L2}>:USE_V4L2>")
target_compile_definitions(compiler_flags INTERFACE "$<$<BOOL:${WITH_V4L2M2M}>:USE_V4L2>")
target_compile_definitions(compiler_flags INTERFACE "$<$<BOOL:${WITH_X11_SCREEN_CAPTURE}>:USE_X11CAPTURE>")
target_compile_definitions(compiler_flags INTERFACE "$<$<BOOL:${WITH_SFOS_SCREEN_CAPTURE}>:USE_LIPSTICK_RECORDER>")
target_compile_definitions(compiler_flags INTERFACE "$<$<BOOL:${WITH_SFOS_HARBOUR}>:USE_SFOS_HARBOUR>")
target_compile_definitions(compiler_flags INTERFACE "$<$<BOOL:${WITH_SFOS}>:USE_SFOS>")
target_compile_definitions(compiler_flags INTERFACE "$<$<BOOL:${WITH_DESKTOP}>:USE_DESKTOP>")
target_compile_definitions(compiler_flags INTERFACE "$<$<BOOL:${WITH_FLATPAK}>:USE_FLATPAK>")
target_compile_definitions(compiler_flags INTERFACE "$<$<BOOL:${WITH_PY}>:USE_PY>")
target_compile_definitions(compiler_flags INTERFACE "$<$<BOOL:${BUILD_PYTHON_MODULE}>:USE_PYTHON_MODULE>")
target_compile_definitions(compiler_flags INTERFACE "$<$<AND:$<CONFIG:Debug>,$<BOOL:${WITH_SANITIZERS}>>:USE_SANITIZERS>")
target_compile_definitions(compiler_flags INTERFACE "$<$<CONFIG:Debug>:DEBUG>")
target_compile_definitions(compiler_flags INTERFACE "$<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:QT_QML_DEBUG>")
target_compile_definitions(compiler_flags INTERFACE "$<$<BOOL:${arch_x8632}>:ARCH_X86_32>")
target_compile_definitions(compiler_flags INTERFACE "$<$<BOOL:${arch_x8664}>:ARCH_X86_64>")
target_compile_definitions(compiler_flags INTERFACE "$<$<BOOL:${arch_arm32}>:ARCH_ARM_32>")
target_compile_definitions(compiler_flags INTERFACE "$<$<BOOL:${arch_arm64}>:ARCH_ARM_64>")
target_compile_definitions(compiler_flags INTERFACE "INSTALL_PREFIX=\"${CMAKE_INSTALL_PREFIX}\"")

# translations

include(${cmake_path}/translations.cmake)

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

if(WITH_DESKTOP)
    find_package(Qt5 COMPONENTS QuickControls2 Widgets REQUIRED)
    list(APPEND deps_libs Qt5::QuickControls2 Qt5::Widgets)
endif()

if(WITH_SFOS)
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
endif()

if(BUILD_PYTHON_MODULE)
    include(${cmake_path}/python_module.cmake)
endif()

if(BUILD_LIBARCHIVE)
    include(${cmake_path}/libarchive.cmake)
else()
    pkg_search_module(libarchive REQUIRED libarchive)
    list(APPEND deps_libs ${libarchive_LIBRARIES})
    list(APPEND includes ${libarchive_INCLUDE_DIRS})
endif()

if(BUILD_XZ)
    include(${cmake_path}/xz.cmake)
else()
    pkg_search_module(xz REQUIRED liblzma>=5.4.1)
    list(APPEND deps_libs ${xz_LIBRARIES})
    list(APPEND includes ${xz_INCLUDE_DIRS})
endif()

if(WITH_DROIDCAM)
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

if (WITH_SFOS_SCREEN_CAPTURE)
    pkg_search_module(wayland REQUIRED wayland-client)
    list(APPEND deps_libs ${wayland_LIBRARIES})
    list(APPEND includes ${wayland_INCLUDE_DIRS} ${Qt5Gui_PRIVATE_INCLUDE_DIRS})
endif()

if (WITH_X11_SCREEN_CAPTURE)
    pkg_search_module(xcb REQUIRED xcb)
    list(APPEND deps_libs ${xcb_LIBRARIES})
    list(APPEND includes ${xcb_INCLUDE_DIRS})

    pkg_search_module(x11 REQUIRED x11)
    list(APPEND deps_libs ${x11_LIBRARIES})
    list(APPEND includes ${x11_INCLUDE_DIRS})
endif()

if(BUILD_FFMPEG)
    include(${cmake_path}/ffmpeg.cmake)
else()
    find_library(lame_path mp3lame)
    list(APPEND deps_libs ${lame_path})

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
        ${ffmpeg_avcodec_LIBRARIES}
        ${ffmpeg_swresample_LIBRARIES}
        ${ffmpeg_swscale_LIBRARIES}
        ${ffmpeg_avutil_LIBRARIES})
    list(APPEND includes
        ${ffmpeg_avutil_INCLUDE_DIRS}
        ${ffmpeg_avcodec_INCLUDE_DIRS}
        ${ffmpeg_avformat_INCLUDE_DIRS}
        ${ffmpeg_avdevice_INCLUDE_DIRS}
        ${ffmpeg_avfilter_INCLUDE_DIRS}
        ${ffmpeg_swresample_INCLUDE_DIRS}
        ${ffmpeg_swscale_INCLUDE_DIRS})
endif()

if(BUILD_GUMBO)
    include(${cmake_path}/gumbo.cmake)
else()
    pkg_search_module(gumbo REQUIRED gumbo)
    list(APPEND deps_libs ${gumbo_LIBRARIES})
    list(APPEND includes ${gumbo_INCLUDE_DIRS})
endif()

if(BUILD_UPNPP)
    include(${cmake_path}/upnpp.cmake)
else()
    pkg_search_module(upnpp REQUIRED libupnpp)
    list(APPEND deps_libs ${upnpp_LIBRARIES} upnpputil)
    list(APPEND includes ${upnpp_INCLUDE_DIRS})

    pkg_search_module(npupnp REQUIRED libnpupnp)
    list(APPEND deps_libs ${npupnp_LIBRARIES})
    list(APPEND includes ${npupnp_INCLUDE_DIRS})

    pkg_search_module(mhd REQUIRED libmicrohttpd)
    list(APPEND deps_libs ${mhd_LIBRARIES})
    list(APPEND includes ${mhd_INCLUDE_DIRS})
endif()

if(BUILD_FMT)
    include(${cmake_path}/fmt.cmake)
else()
    pkg_search_module(fmt REQUIRED fmt)
    list(APPEND deps_libs ${fmt_LIBRARIES})
    list(APPEND includes ${fmt_INCLUDE_DIRS})
endif()

if(BUILD_TAGLIB)
    include(${cmake_path}/taglib.cmake)
else()
    pkg_search_module(taglib REQUIRED taglib)
    list(APPEND deps_libs ${taglib_LIBRARIES})
    list(APPEND includes ${taglib_INCLUDE_DIRS})
endif()

if(NOT ${WITH_SFOS_HARBOUR})
    find_package(Python COMPONENTS Development REQUIRED)
    list(APPEND deps_dirs ${Python_LIBRARY_DIRS})
    list(APPEND deps_libs ${Python_LIBRARIES})
    list(APPEND includes ${Python_INCLUDE_DIRS})

    if(BUILD_PYBIND11)
        include(${cmake_path}/pybind11.cmake)
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

if(WITH_TEST)
    target_include_directories(tests PRIVATE ${includes})
    target_link_libraries(tests ${deps_libs})
    if(deps)
        add_dependencies(tests ${deps})
    endif()
endif()

# install

if(WITH_SFOS)
    include(${cmake_path}/install_sfos.cmake)
endif()

if(WITH_DESKTOP)
    include(${cmake_path}/install_desktop.cmake)
endif()

if(WITH_SFOS)
    file(WRITE "${CMAKE_BINARY_DIR}/QtCreatorDeployment.txt"
        "${CMAKE_INSTALL_PREFIX}\n${CMAKE_BINARY_DIR}/${info_binary_id}:bin\n")
endif()
