//
// Created by ghhg6 on 05.07.2026.
//

#ifndef JAVAUTILS_H
#define JAVAUTILS_H
#include <QJsonObject>
#include <QString>

QString mavenNameToPath(const QString& name);
bool nativeLibraryAllowedOnCurrentOS(const QJsonObject& lib);
void extractNativesForVersion(const QJsonObject& root, const QString& gameDir, const QString& nativesDir);
int requiredJavaMajor(const QString& mcVersion);
int getJavaMajorVersion(const QString& javaExe);
QMap<int, QString> findInstalledJavas();
bool javaIsCompatible(int installedMajor, int requiredMajor);
bool argRulesAllow(const QJsonObject& argObj);
QString mavenKey(const QString& name);
QString pickCompatibleInstalledJava(const QMap<int, QString>& javas, int requiredMajor, const QString& userJavaPath);
QString javaComponentForMajor(int requiredMajor);

#endif //JAVAUTILS_H
