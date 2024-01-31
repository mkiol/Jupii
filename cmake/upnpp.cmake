set(npupnp_source_url "https://www.lesbonscomptes.com/upmpdcli/downloads/libnpupnp-6.1.0.tar.gz")
set(npupnp_checksum "1e305abf63ac945d9cb4576689670c009a914dc9d05b4c1ed605391e7f6b9719")

set(upnpp_source_url "https://www.lesbonscomptes.com/upmpdcli/downloads/libupnpp-0.26.2.tar.gz")
set(upnpp_checksum "d148a522df714ec08b5d03e35c273f340e45eb0d98eb42049247c040814db98f")

set(mhd_source_url "https://ftpmirror.gnu.org/libmicrohttpd/libmicrohttpd-0.9.77.tar.gz")
set(mhd_checksum "9e7023a151120060d2806a6ea4c13ca9933ece4eacfc5c9464d20edddb76b0a0")

ExternalProject_Add(upnpp
    SOURCE_DIR ${external_dir}/upnpp
    BINARY_DIR ${PROJECT_BINARY_DIR}/external/upnpp
    INSTALL_DIR ${PROJECT_BINARY_DIR}/external
    URL ${upnpp_source_url}
    URL_HASH SHA256=${upnpp_checksum}
    CONFIGURE_COMMAND PKG_CONFIG_PATH=${PROJECT_BINARY_DIR}/external/lib/pkgconfig
        <SOURCE_DIR>/configure --prefix=<INSTALL_DIR> --enable-static --disable-shared --with-pic=yes
    BUILD_COMMAND ${MAKE}
    BUILD_ALWAYS False
    INSTALL_COMMAND make DESTDIR=/ install)

# adding prefix npupnp_ to avoid symbol collision with upnpp
set(npupnp_cxxflags "-DMedocUtils=npupnp_MedocUtils")

ExternalProject_Add(npupnp
    SOURCE_DIR ${external_dir}/npupnp
    BINARY_DIR ${PROJECT_BINARY_DIR}/external/npupnp
    INSTALL_DIR ${PROJECT_BINARY_DIR}/external
    URL ${npupnp_source_url}
    URL_HASH SHA256=${npupnp_checksum}
    CONFIGURE_COMMAND PKG_CONFIG_PATH=${PROJECT_BINARY_DIR}/external/lib/pkgconfig
        <SOURCE_DIR>/configure --prefix=<INSTALL_DIR> --enable-static --disable-shared --with-pic=yes
        CXXFLAGS=${npupnp_cxxflags}
    BUILD_COMMAND ${MAKE}
    BUILD_ALWAYS False
    INSTALL_COMMAND make DESTDIR=/ install)

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
    ${external_lib_dir}/libupnpputil.a
    ${external_lib_dir}/libnpupnp.a
    ${external_lib_dir}/libmicrohttpd.a)
list(APPEND deps mhd npupnp upnpp)
