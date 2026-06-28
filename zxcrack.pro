QT += widgets network networkauth svg

CONFIG += c++17

SOURCES += \
    createmodpackwindow.cpp \
    curseforgeclient.cpp \
    curseforgewindow.cpp \
    main.cpp \
    mainwindow.cpp \
    minecraftdownloader.cpp \
    moddetailwindow.cpp \
    modrinthapi.cpp \
    modwindow.cpp \
    settingswindow.cpp

HEADERS += \
    createmodpackwindow.h \
    curseforgeclient.h \
    curseforgewindow.h \
    darktheme.h \
    mainwindow.h \
    minecraftdownloader.h \
    moddetailwindow.h \
    modrinthapi.h \
    modwindow.h \
    settingswindow.h \
    ui_mainwindow.h

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target