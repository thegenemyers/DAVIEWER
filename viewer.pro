QMAKE_SPEC = macx-clang
CONFIG += release
QMAKE_CXXFLAGS += -g -fvisibility=hidden -Wall -Wno-unused-result
QMAKE_CFLAGS += -g -DINTERACTIVE -Wall -Wno-unused-result

MOC_DIR = BUILD
OBJECTS_DIR = BUILD
RCC_DIR = BUILD

QT += widgets

HEADERS       = main_window.h dot_window.h prof_window.h \
                piler.h doter.h DB.h QV.h align.h libfastk.h
SOURCES       = main_window.cpp dot_window.cpp prof_window.cpp \
                main.cpp \
                piler.c doter.c DB.c QV.c align.c libfastk.c
TARGET        = DaViewer
RESOURCES     = viewer.qrc
