cmake_minimum_required(VERSION 3.5)

add_library(qhttpserver STATIC
    http-parser/http_parser.c
    http-parser/http_parser.h
    src/qhttpconnection.cpp
    src/qhttpconnection.h
    src/qhttprequest.cpp
    src/qhttprequest.h
    src/qhttpresponse.cpp
    src/qhttpresponse.h
    src/qhttpserver.cpp
    src/qhttpserver.h
    src/qhttpserverapi.h
    src/qhttpserverfwd.h)

target_include_directories(qhttpserver PRIVATE http-parser src)

find_package(Qt5 COMPONENTS Core Network REQUIRED)
target_link_libraries(qhttpserver Qt5::Core Qt5::Network )
