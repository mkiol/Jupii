set(xz_source_url "https://downloads.sourceforge.net/lzmautils/xz-5.4.6.tar.xz")
set(xz_checksum "b92d4e3a438affcf13362a1305cd9d94ed47ddda22e456a42791e630a5644f5c")

ExternalProject_Add(xz
    SOURCE_DIR ${external_dir}/xz
    BINARY_DIR ${PROJECT_BINARY_DIR}/external/xz
    INSTALL_DIR ${PROJECT_BINARY_DIR}/external
    URL "${xz_source_url}"
    URL_HASH SHA256=${xz_checksum}
    CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=<INSTALL_DIR>
        --enable-xz --disable-xzdec --disable-lzmadec --disable-lzmainfo
        --disable-lzma-links --disable-scripts --disable-doc --disable-shared --enable-static
        --with-pic
    BUILD_COMMAND ${MAKE}
    BUILD_ALWAYS False
    INSTALL_COMMAND make DESTDIR=/ install
)

list(APPEND deps_libs "${external_lib_dir}/liblzma.a")
list(APPEND deps xz)
