# select namespaced vs non-namespaced version
include ($$PWD/../com_github_msorvig_s3.pri)
#include (../qts3.pri)

OBJECTS_DIR = .ob
MOC_DIR = .moc
CONFIG -= app_bundle

SOURCES += example.cpp
