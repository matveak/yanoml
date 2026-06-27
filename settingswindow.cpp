#include "settingswindow.h"
#include <QVBoxLayout>
#include <QCheckBox>
#include <QSlider>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QFileDialog>
#include <QSettings>

SettingsWindow::SettingsWindow(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("Настройки");
    resize(450, 380);  // чуть увеличил высоту

    auto* layout = new QVBoxLayout(this);

    // Снапшоты
    snapshotsCheckBox = new QCheckBox("Показывать снапшоты", this);
    snapshotsCheckBox->setChecked(settings.value("snapshots", false).toBool());

    // Память
    ramLabel = new QLabel(this);
    ramSlider = new QSlider(Qt::Horizontal, this);
    connect(ramSlider, &QSlider::valueChanged, this, [this](int value) {
        ramLabel->setText(QString("Оперативная память: %1 ГБ").arg(value));
        emit settingsChanged();
    });
    ramSlider->setMinimum(1);
    ramSlider->setMaximum(32);
    ramSlider->setValue(settings.value("minecraftRam", 2).toInt());

    // Никнейм
    QLabel* usernameLabel = new QLabel("Никнейм в игре:", this);
    usernameEdit = new QLineEdit(this);
    usernameEdit->setText(settings.value("username", "Player").toString());

    // Путь к Minecraft
    QLabel* minecraftPathLabel = new QLabel("Путь к Minecraft:", this);
    minecraftPathEdit = new QLineEdit(this);
    minecraftBrowseButton = new QPushButton("Обзор...", this);
    minecraftPathEdit->setText(settings.value("minecraftPath",
                                              "C:/Users/" + qgetenv("USERNAME") + "/AppData/Roaming/.minecraft").toString());

    connect(minecraftBrowseButton, &QPushButton::clicked, this, [this] {
        QString dir = QFileDialog::getExistingDirectory(this, "Выберите папку Minecraft");
        if (!dir.isEmpty()) {
            minecraftPathEdit->setText(dir);
            emit settingsChanged();
        }
    });

    // Путь к Java
    QLabel* javaPathLabel = new QLabel("Путь к Java:", this);
    javaPathEdit = new QLineEdit(this);
    javaBrowseButton = new QPushButton("Обзор...", this);
    javaPathEdit->setText(settings.value("javaPath", "C:/Program Files/Java/jdk-21/").toString());

    connect(javaBrowseButton, &QPushButton::clicked, this, [this]{
        QString dir = QFileDialog::getExistingDirectory(this, "Выберите папку Java");
        if (!dir.isEmpty()) {
            javaPathEdit->setText(dir);
            emit settingsChanged();
        }
    });

    // checkStateChanged(Qt::CheckState) появился в Qt 6.7;
    // для совместимости с Qt 5 используем stateChanged(int).
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    connect(snapshotsCheckBox, &QCheckBox::checkStateChanged, this, [this](Qt::CheckState) {
        emit settingsChanged();
    });
#else
    connect(snapshotsCheckBox, &QCheckBox::stateChanged, this, [this](int) {
        emit settingsChanged();
    });
#endif

    // Кнопка сохранения
    QPushButton* closeButton = new QPushButton("Сохранить и закрыть", this);

    // Добавляем всё в layout
    layout->addWidget(snapshotsCheckBox);
    layout->addWidget(ramLabel);
    layout->addWidget(ramSlider);

    layout->addWidget(usernameLabel);
    layout->addWidget(usernameEdit);

    layout->addWidget(minecraftPathLabel);
    layout->addWidget(minecraftPathEdit);
    layout->addWidget(minecraftBrowseButton);

    layout->addWidget(javaPathLabel);
    layout->addWidget(javaPathEdit);
    layout->addWidget(javaBrowseButton);

    layout->addStretch();
    layout->addWidget(closeButton);

    connect(closeButton, &QPushButton::clicked, this, [this]() {
        settings.setValue("minecraftPath", minecraftPathEdit->text());
        settings.setValue("javaPath", javaPathEdit->text());
        settings.setValue("username", usernameEdit->text().trimmed());
        settings.setValue("snapshots", snapshotsCheckBox->isChecked());
        settings.setValue("minecraftRam", ramSlider->value());
        accept();
    });
}

QString SettingsWindow::username() const
{
    QString name = settings.value("username", "Player").toString().trimmed();
    return name.isEmpty() ? "Player" : name;
}

// остальные методы без изменений
bool SettingsWindow::showSnapshots() const { return settings.value("snapshots", false).toBool(); }
int SettingsWindow::ramAmount() const { return settings.value("minecraftRam", 0).toInt(); }
QString SettingsWindow::minecraftPath() const { return settings.value("minecraftPath").toString(); }
QString SettingsWindow::javaPath() const { return settings.value("javaPath").toString() + "/bin/javaw.exe"; }