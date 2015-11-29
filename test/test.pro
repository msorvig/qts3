TEMPLATE = app

include ($$PWD/../qts3.pri)

TARGET = tst_qts3
CONFIG -= app_bundle
OBJECTS_DIR = .ob
MOC_DIR = .moc
QT += testlib

SOURCES += tst_qts3.cpp
