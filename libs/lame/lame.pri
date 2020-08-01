LAME_ROOT = $$PROJECTDIR/libs/lame

INCLUDEPATH += $$LAME_ROOT/include

sailfish {
    HEADERS += $$LAME_ROOT/include/*.h

    x86 {
        LIBS += -L$$LAME_ROOT/build/i486/ -l:libmp3lame.so.0
        liblame.files = $$LAME_ROOT/build/i486/*
    }

    arm {
        LIBS += -L$$LAME_ROOT/build/armv7hl/ -l:libmp3lame.so.0
        liblame.files = $$LAME_ROOT/build/armv7hl/*
    }

    liblame.path = /usr/share/$${TARGET}/lib
    INSTALLS += liblame
}

desktop {
    LIBS += -L$$LAME_ROOT/build/amd64 \
            -l:libmp3lame.a
}
