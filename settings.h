//
// Created by kamanodzu on 20.07.2026.
//

#ifndef YANOML_SETTINGS_H
#define YANOML_SETTINGS_H
#include <QString>

struct Settings {
    bool showSnapshots = false;
    int ramGb = 4;
    QString nickname = "Player";
    QString javaPath = "";
    QString minecraftPath = "";

    void save() const;
    static Settings load();

    //TODO: UI Settings
};

extern Settings globalSettings;

#endif //YANOML_SETTINGS_H
