//
// Created by kamanodzu on 20.07.2026.
//

#include "settings.h"

#include <QSettings>

Settings globalSettings = Settings::load();

void Settings::save() {
    QSettings s("yanoml", "yanoml");
    s.setValue("minecraft-path", minecraftPath);
    s.setValue("java-path", javaPath);
    s.setValue("nickname", nickname);
    s.setValue("show-snapshots", showSnapshots);
    s.setValue("ram-gb", ramGb);
}

Settings Settings::load() {
    QSettings s("yanoml", "yanoml");
    Settings settings;
    settings.minecraftPath = s.value("minecraft-path").toString();
    settings.javaPath = s.value("java-path").toString();
    settings.nickname = s.value("nickname", "Player").toString();
    settings.showSnapshots = s.value("show-snapshots", false).toBool();
    settings.ramGb = s.value("ram-gb", 4).toInt();
    return settings;
}