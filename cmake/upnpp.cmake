set(npupnp_source_url "https://www.lesbonscomptes.com/upmpdcli/downloads/libnpupnp-6.2.0.tar.gz")
set(npupnp_checksum "1c8101607f32a61f2477f23fa33839aa6d64568a83ca23ba43d430290937251d")

set(upnpp_source_url "https://www.lesbonscomptes.com/upmpdcli/downloads/libupnpp-0.26.7.tar.gz")
set(upnpp_checksum "f6630eb2676eeb435882f0e0feca1c9aef63204744016602c53003e14312db00")

set(mhd_source_url "https://ftpmirror.gnu.org/libmicrohttpd/libmicrohttpd-0.9.77.tar.gz")
set(mhd_checksum "9e7023a151120060d2806a6ea4c13ca9933ece4eacfc5c9464d20edddb76b0a0")

unset(meson_bin CACHE)
find_program(meson_bin meson)

if(${meson_bin} MATCHES "-NOTFOUND$")
   message(FATAL_ERROR "meson not found but it is required to build npupnp")
endif()

ExternalProject_Add(upnpp
    SOURCE_DIR ${external_dir}/upnpp
    BINARY_DIR ${PROJECT_BINARY_DIR}/external/upnpp
    INSTALL_DIR ${PROJECT_BINARY_DIR}/external
    URL ${upnpp_source_url}
    URL_HASH SHA256=${upnpp_checksum}
    CONFIGURE_COMMAND PKG_CONFIG_PATH=${external_lib_dir}/pkgconfig
        ${meson_bin} setup --prefix=<INSTALL_DIR>
            --buildtype=release --libdir=lib
            -Ddefault_library=static
            <BINARY_DIR> <SOURCE_DIR>
    BUILD_COMMAND ninja -C <BINARY_DIR>
    BUILD_ALWAYS False
    INSTALL_COMMAND ninja -C <BINARY_DIR> install)

# adding prefix npupnp_ to avoid symbol collision with upnpp
set(npupnp_cxxflags "-DMedocUtils=npupnp_MedocUtils")

ExternalProject_Add(npupnp
    SOURCE_DIR ${external_dir}/npupnp
    BINARY_DIR ${PROJECT_BINARY_DIR}/external/npupnp
    INSTALL_DIR ${PROJECT_BINARY_DIR}/external
    URL ${npupnp_source_url}
    URL_HASH SHA256=${npupnp_checksum}
    CONFIGURE_COMMAND PKG_CONFIG_PATH=${external_lib_dir}/pkgconfig
        ${meson_bin} setup --prefix=<INSTALL_DIR>
            --buildtype=release --libdir=lib
            -Ddefault_library=static
            -Dcpp_args=${npupnp_cxxflags}
            <BINARY_DIR> <SOURCE_DIR>
    BUILD_COMMAND ninja -C <BINARY_DIR>
    BUILD_ALWAYS False
    INSTALL_COMMAND ninja -C <BINARY_DIR> install)

ExternalProject_Add(mhd
    SOURCE_DIR ${external_dir}/mhd
    BINARY_DIR ${PROJECT_BINARY_DIR}/external/mhd
    INSTALL_DIR ${PROJECT_BINARY_DIR}/external
    URL ${mhd_source_url}
    URL_HASH SHA256=${mhd_checksum}
    CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=<INSTALL_DIR>
    --disable-doc --disable-examples --disable-curl --disable-https
    --disable-postprocessor --disable-dauth --disable-bauth --disable-epoll
    --disable-sendfile --disable-httpupgrade
    --enable-static --disable-shared --with-pic=yes
    BUILD_COMMAND ${MAKE}
    BUILD_ALWAYS False
    INSTALL_COMMAND make DESTDIR=/ install)

ExternalProject_Add_StepDependencies(upnpp configure npupnp)
ExternalProject_Add_StepDependencies(npupnp configure mhd)

list(APPEND deps_libs
    ${external_lib_dir}/libupnpp.a
    ${external_lib_dir}/libnpupnp.a
    ${external_lib_dir}/libmicrohttpd.a)
list(APPEND deps mhd npupnp upnpp)
