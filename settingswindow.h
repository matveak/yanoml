#pragma once

#include <QDialog>

class QCheckBox;
class QLabel;
class QSlider;

class SettingsWindow : public QDialog
{
    Q_OBJECT

public:
    ~SettingsWindow();
    explicit SettingsWindow(QWidget* parent = nullptr);

    bool showSnapshots() const;
    int ramAmount() const;

signals:
    void settingsChanged();

private:
    QCheckBox* snapshotsCheckBox = nullptr;
    QSlider* ramSlider = nullptr;
    QLabel* ramLabel = nullptr;
};