//
// Created by ghhg6 on 05.07.2026.
//

#include "Zip.h"

QVector<ZipEntry> zipCentralDir(QFile& f)
{
    QVector<ZipEntry> entries;
    const qint64 sz = f.size();
    if (sz < 22) return entries;

    // Ищем EOCD сигнатуру 0x06054b50 с конца файла.
    const qint64 searchLen = qMin((qint64)65557, sz);
    f.seek(sz - searchLen);
    const QByteArray tail = f.read(searchLen);

    int eocd = -1;
    for (int i = (int)tail.size() - 22; i >= 0; --i)
    {
        if ((quint8)tail[i]==0x50 && (quint8)tail[i+1]==0x4b &&
            (quint8)tail[i+2]==0x05 && (quint8)tail[i+3]==0x06)
        { eocd = i; break; }
    }
    if (eocd < 0) return entries;

    const quint8* e = (const quint8*)tail.constData() + eocd;
    quint16 num   = e[8]  | (e[9]  << 8);
    quint32 cdOff = e[16] | (e[17]<<8) | (e[18]<<16) | (e[19]<<24);

    if (!f.seek(cdOff)) return entries;

    for (int i = 0; i < num; ++i)
    {
        QByteArray hdr = f.read(46);
        if (hdr.size() < 46) break;
        const quint8* h = (const quint8*)hdr.constData();
        if (h[0]!=0x50||h[1]!=0x4b||h[2]!=0x01||h[3]!=0x02) break;

        quint16 nl  = h[28]|(h[29]<<8);
        quint16 el  = h[30]|(h[31]<<8);
        quint16 cl  = h[32]|(h[33]<<8);
        quint32 off = h[42]|(h[43]<<8)|(h[44]<<16)|(h[45]<<24);

        ZipEntry ze;
        ze.name        = QString::fromUtf8(f.read(nl));
        ze.localOffset = off;
        f.skip(el + cl);
        entries.append(ze);
    }
    return entries;
}

// Читает данные одного файла из local-заголовка (stored или deflate).
QByteArray zipReadEntry(QFile& f, quint32 localOff)
{
    if (!f.seek(localOff)) return {};
    QByteArray lh = f.read(30);
    if (lh.size() < 30) return {};
    const quint8* h = (const quint8*)lh.constData();
    if (h[0]!=0x50||h[1]!=0x4b||h[2]!=0x03||h[3]!=0x04) return {};

    quint16 method   = h[8] |(h[9] <<8);
    quint32 compSz   = h[18]|(h[19]<<8)|(h[20]<<16)|(h[21]<<24);
    quint32 uncompSz = h[22]|(h[23]<<8)|(h[24]<<16)|(h[25]<<24);
    quint16 nl       = h[26]|(h[27]<<8);
    quint16 el       = h[28]|(h[29]<<8);
    f.skip(nl + el);

    QByteArray data = f.read(compSz);

    if (method == 0)          // stored
        return data;

    if (method == 8)          // deflate → оборачиваем в zlib-обёртку для qUncompress
    {
        // qUncompress ждёт 4 байта big-endian несжатого размера + zlib-поток
        QByteArray zlibStream;
        zlibStream.resize(4);
        quint8* sz4 = (quint8*)zlibStream.data();
        sz4[0] = (uncompSz>>24)&0xff; sz4[1] = (uncompSz>>16)&0xff;
        sz4[2] = (uncompSz>> 8)&0xff; sz4[3] =  uncompSz     &0xff;
        // zlib-заголовок (CMF=0x78 FLG=0x9C) + raw deflate + adler32 заглушка
        zlibStream += (char)0x78; zlibStream += (char)0x9C;
        zlibStream += data;
        zlibStream += QByteArray(4, '\x00'); // adler32 (игнорируется qUncompress)
        QByteArray out = qUncompress(zlibStream);
        return out;
    }

    qWarning() << "Unsupported ZIP compression method:" << method;
    return {};
}