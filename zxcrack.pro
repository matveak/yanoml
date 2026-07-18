QT += widgets network networkauth svg

CONFIG += c++17

SOURCES += \
    JavaUtils.cpp \
    MinecraftLauncher.cpp \
    Zip.cpp \
    curseforgeclient.cpp \
    main.cpp \
    minecraftdownloader.cpp \
    modrinthapi.cpp \
    windows/createmodpackwindow.cpp \
    windows/curseforgewindow.cpp \
    windows/mainwindow.cpp \
    windows/moddetailwindow.cpp \
    windows/modwindow.cpp \
    windows/settingswindow.cpp \
    windows/solitaireGame.cpp \
    ui/theme.cpp \
    ui/titlebar.cpp \
    ui/windowframe.cpp

HEADERS += \
    JavaUtils.h \
    MinecraftLauncher.h \
    Zip.h \
    curseforgeclient.h \
    minecraftdownloader.h \
    modrinthapi.h \
    windows/createmodpackwindow.h \
    windows/curseforgewindow.h \
    windows/mainwindow.h \
    windows/moddetailwindow.h \
    windows/modwindow.h \
    windows/settingswindow.h \
    windows/solitaireGame.h \
    windows/ui_mainwindow.h \
    ui/theme.h \
    ui/titlebar.h \
    ui/windowframe.h

RESOURCES += \
    resources.qrc

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
