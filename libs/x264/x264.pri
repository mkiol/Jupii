X264_ROOT = $$PROJECTDIR/libs/x264

sailfish {
    x86 {
        LIBS += -L$$X264_ROOT/build/i486/ -l:libx264.so.157
    }
    arm {
        LIBS += -L$$X264_ROOT/build/armv7hl/ -l:libx264.so.157
    }

    x86: libx264.files = $$X264_ROOT/build/i486/*
    arm: libx264.files = $$X264_ROOT/build/armv7hl/*
    libx264.path = /usr/share/$${TARGET}/lib
    INSTALLS += libx264
}

desktop {
    static_x264 {
        LIBS += -L$$X264_ROOT/build/amd64/ -l:libx264.a
        LIBS += -ldl
    } else {
        LIBS += -lx264
    }
}
