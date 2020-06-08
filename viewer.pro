QMAKE_SPEC = macx-clang
CONFIG += release
QMAKE_CXXFLAGS += -O3 -fvisibility=hidden -Wall -Wno-unused-result
QMAKE_CFLAGS += -O3 -DINTERACTIVE -Wall -Wno-unused-result

MOC_DIR = BUILD
OBJECTS_DIR = BUILD
RCC_DIR = BUILD

QT += widgets

HEADERS       = main_window.h dot_window.h \
                piler.h doter.h DB.h QV.h align.h
SOURCES       = main_window.cpp dot_window.cpp \
                main.cpp \
                piler.c doter.c DB.c QV.c align.c
TARGET        = DaViewer
RESOURCES     = viewer.qrc
