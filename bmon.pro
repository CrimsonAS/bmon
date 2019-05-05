TEMPLATE = app
TARGET = bmon
QT += quick widgets
INCLUDEPATH += .

DEFINES += QT_DEPRECATED_WARNINGS

# Input
SOURCES += \
    bmon.cpp \
    processmodel.cpp \
    sortfilterproxymodel.cpp

HEADERS += \
    processmodel.h \
    sortfilterproxymodel.h
