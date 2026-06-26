QT += widgets network networkauth gui-private


CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    createmodpackwindow.cpp \
    main.cpp \
    mainwindow.cpp \
    minecraftdownloader.cpp \
    moddetailwindow.cpp \
    modrinthapi.cpp \
    modwindow.cpp \
    settingswindow.cpp

HEADERS += \
    createmodpackwindow.h \
    mainwindow.h \
    minecraftdownloader.h \
    moddetailwindow.h \
    modrinthapi.h \
    modwindow.h \
    settingswindow.h \
    ui_mainwindow.h

FORMS += \
    mainwindow.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
