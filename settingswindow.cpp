#include "settingswindow.h"

#include <QVBoxLayout>
#include <QCheckBox>
#include <QSlider>
#include <QLabel>
#include <QPushButton>


SettingsWindow::~SettingsWindow()
{
}

SettingsWindow::SettingsWindow(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("Настройки");
    resize(400, 250);

    auto* layout =
        new QVBoxLayout(this);

    snapshotsCheckBox =
        new QCheckBox(
            "Показывать снапшоты",
            this);

    connect(snapshotsCheckBox,
            &QCheckBox::stateChanged,
            this,
            [this](int)
            {
                emit settingsChanged();
            });

    ramLabel =
        new QLabel(
            "Оперативная память: 1024 МБ",
            this);

    ramSlider =
        new QSlider(
            Qt::Horizontal,
            this);

    ramSlider->setMinimum(1);
    ramSlider->setMaximum(32);
    ramSlider->setValue(1);

    connect(ramSlider,
            &QSlider::valueChanged,
            this,
            [this](int value)
            {
                int ramMb =
                    value * 1024;

                ramLabel->setText(
                    QString(
                        "Оперативная память: %1 ГБ (%2 МБ)")
                        .arg(value)
                        .arg(ramMb));

                emit settingsChanged();
            });

    QPushButton* closeButton =
        new QPushButton(
            "Закрыть",
            this);

    connect(closeButton,
            &QPushButton::clicked,
            this,
            &QDialog::accept);

    layout->addWidget(
        snapshotsCheckBox);

    layout->addWidget(
        ramLabel);

    layout->addWidget(
        ramSlider);

    layout->addStretch();

    layout->addWidget(
        closeButton);
}

bool SettingsWindow::showSnapshots() const
{
    return snapshotsCheckBox &&
           snapshotsCheckBox->isChecked();
}

int SettingsWindow::ramAmount() const
{
    return ramSlider
               ? ramSlider->value() * 1024
               : 1024;
}