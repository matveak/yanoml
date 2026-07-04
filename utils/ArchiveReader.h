//
// Created by ghhg6 on 04.07.2026.
//

#ifndef ARCHIVEREADER_H
#define ARCHIVEREADER_H
#include <QString>

struct ArchiveEntry {
	QString name;
	quint64 localOffset;
};

class ArchiveReader {
	public:
		//TODO: do shit
};

#endif //ARCHIVEREADER_H
