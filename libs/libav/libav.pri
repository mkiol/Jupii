INCLUDEPATH += $$PROJECTDIR/libs/libav/src

x86 {
    LIBS += -L$$PROJECTDIR/libs/libav/build/i486/ -l:libavutil.so.55
    LIBS += -L$$PROJECTDIR/libs/libav/build/i486/ -l:libavformat.so.57
    LIBS += -L$$PROJECTDIR/libs/libav/build/i486/ -l:libavcodec.so.57
    LIBS += -L$$PROJECTDIR/libs/libav/build/i486/ -l:libavresample.so.3
}

arm {
    LIBS += -L$$PROJECTDIR/libs/libav/build/armv7hl/ -l:libavutil.so.55
    LIBS += -L$$PROJECTDIR/libs/libav/build/armv7hl/ -l:libavformat.so.57
    LIBS += -L$$PROJECTDIR/libs/libav/build/armv7hl/ -l:libavcodec.so.57
    LIBS += -L$$PROJECTDIR/libs/libav/build/armv7hl/ -l:libavresample.so.3
}

x86: libav.files = $$PROJECTDIR/libs/libav/build/i486/*
arm: libav.files = $$PROJECTDIR/libs/libav/build/armv7hl/*
libav.path = /usr/share/$${TARGET}/lib
INSTALLS += libav
