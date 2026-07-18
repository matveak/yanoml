void MainWindow::startLoaderInstall(const QString& loader,
                                    const QString& mcVersion,
                                    const QString& gameDir)
{
    progressBar->setValue(0);
    progressBar->show();

    auto conn = std::make_shared<QMetaObject::Connection>();
    *conn = connect(downloader, &MinecraftDownloader::loaderInstalled, this,
                    [this, conn](const QString& versionId)
                    {
                        disconnect(*conn);
                        m_modLoaderPending = false;
                        progressBar->hide();
                        QMessageBox::information(this, "Done",
                                                 "Loader installed:\n" + versionId);
                    });

    if (loader == "fabric")
    {
        downloader->installFabric(mcVersion, gameDir);
    }
    else
    {
        launcher->ensureJava(mcVersion, gameDir,
                             [this, loader, mcVersion, gameDir](const QString& javaExe)
                             {
                                 progressBar->hide();
                                 downloader->installForgeLike(mcVersion, loader, javaExe, gameDir);
                             }, settingsWindow->javaPath());
    }
}
