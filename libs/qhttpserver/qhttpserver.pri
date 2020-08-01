QHTTPSERVER_ROOT = $$PROJECTDIR/libs/qhttpserver

INCLUDEPATH += $$QHTTPSERVER_ROOT/http-parser $$QHTTPSERVER_ROOT/src

PRIVATE_HEADERS += $$QHTTPSERVER_ROOT/http-parser/http_parser.h \
                   $$QHTTPSERVER_ROOT/src/qhttpconnection.h

PUBLIC_HEADERS += $$QHTTPSERVER_ROOT/src/qhttpserver.h \
                  $$QHTTPSERVER_ROOT/src/qhttprequest.h \
                  $$QHTTPSERVER_ROOT/src/qhttpresponse.h \
                  $$QHTTPSERVER_ROOT/src/qhttpserverapi.h \
                  $$QHTTPSERVER_ROOT/src/qhttpserverfwd.h

HEADERS = $$PRIVATE_HEADERS $$PUBLIC_HEADERS

SOURCES += \
    $$QHTTPSERVER_ROOT/http-parser/http_parser.c \
    $$QHTTPSERVER_ROOT/src/qhttpconnection.cpp \
    $$QHTTPSERVER_ROOT/src/qhttprequest.cpp \
    $$QHTTPSERVER_ROOT/src/qhttpresponse.cpp \
    $$QHTTPSERVER_ROOT/src/qhttpserver.cpp
