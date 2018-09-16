QHTTPSERVER_BASE = ../qhttpserver

INCLUDEPATH += $$QHTTPSERVER_BASE/http-parser $$QHTTPSERVER_BASE/src

PRIVATE_HEADERS += $$QHTTPSERVER_BASE/http-parser/http_parser.h \
                   $$QHTTPSERVER_BASE/src/qhttpconnection.h

PUBLIC_HEADERS += $$QHTTPSERVER_BASE/src/qhttpserver.h \
                  $$QHTTPSERVER_BASE/src/qhttprequest.h \
                  $$QHTTPSERVER_BASE/src/qhttpresponse.h \
                  $$QHTTPSERVER_BASE/src/qhttpserverapi.h \
                  $$QHTTPSERVER_BASE/src/qhttpserverfwd.h

HEADERS = $$PRIVATE_HEADERS $$PUBLIC_HEADERS

SOURCES += \
    $$QHTTPSERVER_BASE/http-parser/http_parser.c \
    $$QHTTPSERVER_BASE/src/qhttpconnection.cpp \
    $$QHTTPSERVER_BASE/src/qhttprequest.cpp \
    $$QHTTPSERVER_BASE/src/qhttpresponse.cpp \
    $$QHTTPSERVER_BASE/src/qhttpserver.cpp
