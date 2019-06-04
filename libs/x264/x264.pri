sailfish {
    x86 {
        LIBS += -L$$PROJECTDIR/libs/x264/build/i486/ -l:libx264.so.157
    }
    arm {
        LIBS += -L$$PROJECTDIR/libs/x264/build/armv7hl/ -l:libx264.so.157
    }

    x86: libx264.files = $$PROJECTDIR/libs/x264/build/i486/*
    arm: libx264.files = $$PROJECTDIR/libs/x264/build/armv7hl/*
    libx264.path = /usr/share/$${TARGET}/lib
    INSTALLS += libx264
}
