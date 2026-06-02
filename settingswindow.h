#pragma once

#include <QDialog>
#include <QSettings>
#include <QString>

class QCheckBox;
class QLabel;
class QSlider;
class QLineEdit;
class QPushButton;

class SettingsWindow : public QDialog{
    Q_OBJECT

public:
    explicit SettingsWindow(QWidget* parent = nullptr);

    bool showSnapshots() const;
    int ramAmount() const;
    QString minecraftPath() const;
    QString javaPath() const;

    signals:
        void settingsChanged();

private:
    QCheckBox* snapshotsCheckBox = nullptr;
    QSlider* ramSlider = nullptr;
    QLabel* ramLabel = nullptr;

    QLineEdit* minecraftPathEdit = nullptr;
    QPushButton* minecraftBrowseButton = nullptr;

    QLineEdit* javaPathEdit = nullptr;
    QPushButton* javaBrowseButton = nullptr;
    QSettings settings{"MyLauncher", "Crack"};
};