QMAKE_SPEC = macx-clang
CONFIG += release
QMAKE_CXXFLAGS += -fvisibility=hidden -Wall -Wno-unused-result
QMAKE_CFLAGS += -DINTERACTIVE -Wall -Wno-unused-result

MOC_DIR = BUILD
OBJECTS_DIR = BUILD
RCC_DIR = BUILD

QT += widgets

HEADERS       = main_window.h \
                piler.h DB.h QV.h align.h
SOURCES       = main_window.cpp \
                main.cpp \
                piler.c DB.c QV.c align.c
TARGET        = DaViewer
RESOURCES     = viewer.qrc
