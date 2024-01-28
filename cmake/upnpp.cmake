set(npupnp_source_url "https://www.lesbonscomptes.com/upmpdcli/downloads/libnpupnp-5.0.0.tar.gz")
set(npupnp_checksum "2e5648cf180a425ef57b8c9c0d9dbd77f0314487ea0e0a85ebc6c3ef87cab05b")

set(upnpp_source_url "https://www.lesbonscomptes.com/upmpdcli/downloads/libupnpp-0.22.4.tar.gz")
set(upnpp_checksum "37deca94176df8becead110a0b5cd11fe1f9c1c351e4c15a05570bf1c6fec09c")

set(mhd_source_url "https://ftpmirror.gnu.org/libmicrohttpd/libmicrohttpd-0.9.75.tar.gz")
set(mhd_checksum "9278907a6f571b391aab9644fd646a5108ed97311ec66f6359cebbedb0a4e3bb")

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

ExternalProject_Add(npupnp
    SOURCE_DIR ${external_dir}/npupnp
    BINARY_DIR ${PROJECT_BINARY_DIR}/external/npupnp
    INSTALL_DIR ${PROJECT_BINARY_DIR}/external
    URL ${npupnp_source_url}
    URL_HASH SHA256=${npupnp_checksum}
    CONFIGURE_COMMAND PKG_CONFIG_PATH=${PROJECT_BINARY_DIR}/external/lib/pkgconfig
        <SOURCE_DIR>/configure --prefix=<INSTALL_DIR> --enable-static --disable-shared --with-pic=yes
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
