INCLUDEPATH += $$PROJECTDIR/libs/lame/include

x86 {
    LIBS += -L$$PROJECTDIR/libs/lame/build/i486/ -l:libmp3lame.so.0
}
arm {
    LIBS += -L$$PROJECTDIR/libs/lame/build/armv7hl/ -l:libmp3lame.so.0
}

sailfish {
    x86: liblame.files = $$PROJECTDIR/libs/lame/build/i486/*
    arm: liblame.files = $$PROJECTDIR/libs/lame/build/armv7hl/*
    liblame.path = /usr/share/$${TARGET}/lib
    INSTALLS += liblame
}

HEADERS += $$PROJECTDIR/libs/lame/include/*.h
