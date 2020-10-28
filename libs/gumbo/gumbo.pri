GUMBO_ROOT = $$PROJECTDIR/libs/gumbo

INCLUDEPATH += $$GUMBO_ROOT/include
HEADERS += $$GUMBO_ROOT/include/gumbo.h

desktop {
    LIBS += -L$$GUMBO_ROOT/build/amd64 \
            -l:libgumbo.a
}
