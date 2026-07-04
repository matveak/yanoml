//
// Created by ghhg6 on 04.07.2026.
//

#ifndef JAVADOWNLOADER_H
#define JAVADOWNLOADER_H

#include "downloader.h"

class JavaDownloader : public Downloader {
	public:
	JavaDownloader();


	void downloadJavaRuntime(const QString &component, const QString &outputDir);

	signals:
	void javaRuntimeProgress(int percent);
	void javaRuntimeReady(const QString& javaExecutable);
};

#endif //JAVADOWNLOADER_H
