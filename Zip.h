//
// Created by ghhg6 on 05.07.2026.
//

#ifndef ZIP_H
#define ZIP_H
#include <QFile>
#include <QString>
#include <qtypes.h>
#include <QByteArray>
#include <QDebug>

struct ZipEntry { QString name; quint32 localOffset; };

QVector<ZipEntry> zipCentralDir(QFile& f);
QByteArray zipReadEntry(QFile& f, quint32 localOff);


#endif //ZIP_H
