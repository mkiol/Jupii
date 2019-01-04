TAGLIB_HEADERS_ROOT = $$PROJECTDIR/libs/taglib/taglib

INCLUDEPATH += \
    $$TAGLIB_HEADERS_ROOT \
    $$TAGLIB_HEADERS_ROOT/toolkit \
    $$TAGLIB_HEADERS_ROOT/mpeg \
    $$TAGLIB_HEADERS_ROOT/mpeg/id3v1 \
    $$TAGLIB_HEADERS_ROOT/mpeg/id3v2 \
    $$TAGLIB_HEADERS_ROOT/mpeg/id3v2/frames \
    $$TAGLIB_HEADERS_ROOT/ogg \
    $$TAGLIB_HEADERS_ROOT/ogg/vorbis \
    $$TAGLIB_HEADERS_ROOT/ogg/flac \
    $$TAGLIB_HEADERS_ROOT/ogg/speex \
    $$TAGLIB_HEADERS_ROOT/ogg/opus \
    $$TAGLIB_HEADERS_ROOT/flac \
    $$TAGLIB_HEADERS_ROOT/ape \
    $$TAGLIB_HEADERS_ROOT/mpc \
    $$TAGLIB_HEADERS_ROOT/wavpack \
    $$TAGLIB_HEADERS_ROOT/trueaudio \
    $$TAGLIB_HEADERS_ROOT/riff \
    $$TAGLIB_HEADERS_ROOT/riff/aiff \
    $$TAGLIB_HEADERS_ROOT/riff/wav \
    $$TAGLIB_HEADERS_ROOT/asf \
    $$TAGLIB_HEADERS_ROOT/mp4 \
    $$TAGLIB_HEADERS_ROOT/mod \
    $$TAGLIB_HEADERS_ROOT/it \
    $$TAGLIB_HEADERS_ROOT/s3m \
    $$TAGLIB_HEADERS_ROOT/xm

x86 {
    LIBS += -L$$PROJECTDIR/libs/taglib/build/i486/ -l:libtag.so.1
}
arm {
    LIBS += -L$$PROJECTDIR/libs/taglib/build/armv7hl/ -l:libtag.so.1
}

x86: libtaglib.files = $$PROJECTDIR/libs/taglib/build/i486/*
arm: libtaglib.files = $$PROJECTDIR/libs/taglib/build/armv7hl/*
libtaglib.path = /usr/share/$${TARGET}/lib
INSTALLS += libtaglib

HEADERS += \
    $$TAGLIB_HEADERS_ROOT/*.h \
    $$TAGLIB_HEADERS_ROOT/toolkit/*.h \
    $$TAGLIB_HEADERS_ROOT/mpeg/*.h \
    $$TAGLIB_HEADERS_ROOT/mpeg/id3v1/*.h \
    $$TAGLIB_HEADERS_ROOT/mpeg/id3v2/*.h \
    $$TAGLIB_HEADERS_ROOT/mpeg/id3v2/frames/*.h \
    $$TAGLIB_HEADERS_ROOT/ogg/*.h \
    $$TAGLIB_HEADERS_ROOT/ogg/vorbis/*.h \
    $$TAGLIB_HEADERS_ROOT/ogg/flac/*.h \
    $$TAGLIB_HEADERS_ROOT/ogg/speex/*.h \
    $$TAGLIB_HEADERS_ROOT/ogg/opus/*.h \
    $$TAGLIB_HEADERS_ROOT/flac/*.h \
    $$TAGLIB_HEADERS_ROOT/ape/*.h \
    $$TAGLIB_HEADERS_ROOT/mpc/*.h \
    $$TAGLIB_HEADERS_ROOT/wavpack/*.h \
    $$TAGLIB_HEADERS_ROOT/trueaudio/*.h \
    $$TAGLIB_HEADERS_ROOT/riff/*.h \
    $$TAGLIB_HEADERS_ROOT/riff/aiff/*.h \
    $$TAGLIB_HEADERS_ROOT/riff/wav/*.h \
    $$TAGLIB_HEADERS_ROOT/asf/*.h \
    $$TAGLIB_HEADERS_ROOT/mp4/*.h \
    $$TAGLIB_HEADERS_ROOT/mod/*.h \
    $$TAGLIB_HEADERS_ROOT/it/*.h \
    $$TAGLIB_HEADERS_ROOT/s3m/*.h \
    $$TAGLIB_HEADERS_ROOT/xm/*.h

