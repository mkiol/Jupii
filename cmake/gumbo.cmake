set(gumbo_source_url "https://github.com/google/gumbo-parser/archive/refs/tags/v0.10.1.tar.gz")
set(gumbo_checksum "28463053d44a5dfbc4b77bcf49c8cee119338ffa636cc17fc3378421d714efad")

ExternalProject_Add(gumbo
    SOURCE_DIR ${external_dir}/gumbo
    BINARY_DIR ${PROJECT_BINARY_DIR}/external/gumbo
    INSTALL_DIR ${PROJECT_BINARY_DIR}/external
    URL ${gumbo_source_url}
    URL_HASH SHA256=${gumbo_checksum}
    CONFIGURE_COMMAND sh -c "cd <SOURCE_DIR> && <SOURCE_DIR>/autogen.sh" &&
        <SOURCE_DIR>/configure --prefix=<INSTALL_DIR>
        --enable-static --disable-shared --with-pic=yes
    BUILD_COMMAND ${MAKE}
    BUILD_ALWAYS False
    INSTALL_COMMAND make DESTDIR=/ install)

list(APPEND deps_libs ${external_lib_dir}/libgumbo.a)
list(APPEND deps gumbo)

