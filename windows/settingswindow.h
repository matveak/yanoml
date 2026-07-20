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

signals:
    void settingsChanged();

private:
    QCheckBox   *m_snapshotsCheckBox     = nullptr;
    QSlider     *m_ramSlider             = nullptr;
    QLabel      *m_ramLabel              = nullptr;
    QLineEdit   *m_minecraftPathEdit     = nullptr;
    QPushButton *m_minecraftBrowseButton = nullptr;
    QLineEdit   *m_javaPathEdit          = nullptr;
    QPushButton *m_javaBrowseButton      = nullptr;
    QLineEdit   *m_nicknameEdit          = nullptr;   // ← Новое поле
};