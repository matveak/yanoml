#pragma once

#include "modrinthapi.h"
#include <QDialog>
#include <QVector>
#include <QEvent>

class QLineEdit;
class QPushButton;
class QScrollArea;
class QWidget;
class QVBoxLayout;

class ModWindow : public QDialog
{
    Q_OBJECT

public:
    explicit ModWindow(QWidget *parent = nullptr);

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private slots: // QVector
    void onDownloadLinks(const QVector<QUrl>& urls);

private:
    QLineEdit* searchEdit = nullptr;
    QPushButton* searchButton = nullptr;
    QScrollArea* scrollArea = nullptr;
    QWidget* cardsWidget = nullptr;
    QVBoxLayout* cardsLayout = nullptr;
    QPushButton* installButton = nullptr;

    ModrithAPI* api = nullptr;

    QVector<Mod> cachedMods;

    void clearCards();
    void addModCards(const QList<Mod>& mods);
    void installSelectedMod();
};
