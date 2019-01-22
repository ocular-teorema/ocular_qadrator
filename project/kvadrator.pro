QT += core
QT -= gui
QT += websockets

TARGET = kvadrator
CONFIG += console
CONFIG -= app_bundle

TEMPLATE = app

INCLUDEPATH += /usr/include

SOURCES += ../src/main.cpp \
    ../src/errorHandler.cpp \
    ../src/frameBuffer.cpp \
    ../src/kvadrator.cpp \
    ../src/output.cpp \
    ../src/stream.cpp \
    ../src/builder.cpp \
    ../src/videoScaler.cpp \
    ../src/videoEncoder.cpp

HEADERS += \
    ../src/errorHandler.h \
    ../src/frameBuffer.h \
    ../src/kvadrator.h \
    ../src/output.h \
    ../src/stream.h \
    ../src/common.h \
    ../src/builder.h \
    ../src/videoScaler.h \
    ../src/videoEncoder.h

QMAKE_CXXFLAGS += -std=c++11

LIBS +=  -L/usr/lib
LIBS +=  -lavformat -lavcodec -lavutil -lswresample -lswscale -lxcb-xfixes -lxcb-render -lxcb-shape -lxcb -lX11 -lx264 -lm -lz
LIBS +=  -lopencv_core
