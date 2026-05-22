#pragma once

#include <QDialog>
#include <QString>

class QCheckBox;
class QLabel;
class QSlider;
class QLineEdit;
class QPushButton;

class SettingsWindow : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsWindow(QWidget* parent = nullptr);
    ~SettingsWindow();

    bool showSnapshots() const;
    int ramAmount() const;
    QString minecraftPath() const;   // ← Новый метод

signals:
    void settingsChanged();

private:
    QCheckBox* snapshotsCheckBox = nullptr;
    QSlider* ramSlider = nullptr;
    QLabel* ramLabel = nullptr;

    QLineEdit* pathEdit = nullptr;      // ← Поле для пути
    QPushButton* browseButton = nullptr; // ← Кнопка "Обзор"
};