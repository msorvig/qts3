# include this file if you want to inlcude QtS3 in the global namespace,
# include com_github_msorvig_s3.pri if you want want the QPM namespaceing.

INCLUDEPATH += $$PWD

QT += core network

HEADERS += \
    $$PWD/qts3.h \
    $$PWD/qts3qnam_p.h \
    $$PWD/qts3_p.h \
    
SOURCES += \
    $$PWD/qts3.cpp \
    $$PWD/qts3qnam.cpp \
    $$PWD/qts3_p.cpp \
