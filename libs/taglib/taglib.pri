#
# SFOS:
#
# wget https://github.com/taglib/taglib/archive/v1.11.1.tar.gz
# tar -xf v1.11.1.tar.gz
# cd taglib-1.11.1
# mkdir build
# cd build
# sb2 -t SailfishOS-[target] cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON -DENABLE_STATIC_RUNTIME=OFF ../
# sb2 -t SailfishOS-[target] make
# cp -L taglib/libtag.so.1 [dest]
#

TAGLIB_ROOT = $$PROJECTDIR/libs/taglib

INCLUDEPATH += $$TAGLIB_ROOT/include \
               $$TAGLIB_ROOT/include/taglib

sailfish {
    x86 {
        LIBS += -L$$TAGLIB_ROOT/build/i486/ -l:libtag.so.1
        libtaglib.files = $$TAGLIB_ROOT/build/i486/*
    }

    arm {
        LIBS += -L$$TAGLIB_ROOT/build/armv7hl/ -l:libtag.so.1
        libtaglib.files = $$TAGLIB_ROOT/build/armv7hl/*
    }

    libtaglib.path = /usr/share/$${TARGET}/lib
    INSTALLS += libtaglib
}

desktop {
    amd64 {
        LIBS += -L$$TAGLIB_ROOT/build/amd64 \
                -l:libtag.a
    }

    arm64 {
        LIBS += -L$$TAGLIB_ROOT/build/arm64 \
                -l:libtag.a
    }
}
