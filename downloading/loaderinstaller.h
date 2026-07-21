//
// Created by kamanodzu on 21.07.2026.
//

#ifndef YANOML_LOADERINSTALLER_H
#define YANOML_LOADERINSTALLER_H
#include <QString>


class LoaderInstaller {
    static QString findInstalledLoaderId(const QString& gameDir, const QString& loader, const QString& mcVersion);
    void installFabric(const QString& mcVersion, const QString& gameDir);
    void installForgeLike(const QString& mcVersion, const QString& loader, const QString& javaExe, const QString& gameDir);
    void runLoaderInstaller(const QUrl& installerUrl, const QString& mcVersion, const QString& loader, const QString& javaExe, const QString& gameDir);
};


#endif //YANOML_LOADERINSTALLER_H
