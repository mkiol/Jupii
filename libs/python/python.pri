PYTHON_ROOT = $$PROJECTDIR/libs/python

INCLUDEPATH += $$PROJECTDIR/external/pybind11/include
PKGCONFIG += python3

sailfish {
    LIBS += -lutil
    LIBS += -L$${PYTHON_ROOT}/sfos-$${ARCH_PREFIX} \
            -l:libpython3.8.so.1.0 \
            -l:liblzma.so.5 \
            -l:libarchive.so.13

    python.files = $${PYTHON_ROOT}/sfos-$${ARCH_PREFIX}/*.so.* \
                   $${PYTHON_ROOT}/sfos-$${ARCH_PREFIX}/python.tar.xz
    python.path = /usr/share/$${TARGET}/lib
    INSTALLS += python
}

desktop {
    LIBS += -lutil -lpython3.10 -llzma -larchive
}
