#include "titlebar.h"

#include "theme.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QWindow>

TitleBar::TitleBar(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("titleBar");
    setFixedHeight(36);

    auto layout = new QHBoxLayout(this);

    layout->setContentsMargins(8, 0, 8, 0);
    layout->setSpacing(4);

    m_left = new QWidget(this);
    auto leftLayout = new QHBoxLayout(m_left);
    leftLayout->setContentsMargins(0,0,0,0);
    leftLayout->setSpacing(4);

    m_right = new QWidget(this);
    auto rightLayout = new QHBoxLayout(m_right);
    rightLayout->setContentsMargins(0,0,0,0);
    rightLayout->setSpacing(2);

    m_title = new QLabel(windowTitle(), this);
    m_title->setAlignment(Qt::AlignCenter);

    auto minimizeButton = new QPushButton("-", this);
    auto maximizeButton = new QPushButton("□", this);
    auto closeButton = new QPushButton("✕", this);

    QList<QPushButton*> buttons =
    {
        minimizeButton,
        maximizeButton,
        closeButton
    };

    for (auto button : buttons)
    {
        button->setFixedSize(32, 28);
        button->setCursor(Qt::PointingHandCursor);
        button->setFlat(true);

        button->setStyleSheet(QString(R"(

QPushButton
{
    border:none;
    border-radius:6px;
    background:transparent;
    color:%1;
}

QPushButton:hover
{
    background:%2;
}

QPushButton:pressed
{
    background:%3;
}

)")
            .arg(Theme::text().name())
            .arg(Theme::panelHighlight().name())
            .arg(Theme::panel().name()));

        rightLayout->addWidget(button);
    }

    closeButton->setStyleSheet(QString(R"(

QPushButton
{
    border:none;
    border-radius:6px;
    background:transparent;
    color:%1;
}

QPushButton:hover
{
    background:#E81123;
    color:white;
}

QPushButton:pressed
{
    background:#C50F1F;
    color:white;
}

)")
        .arg(Theme::text().name()));

    layout->addWidget(m_left);

    layout->addStretch();

    layout->addWidget(m_title);

    layout->addStretch();

    layout->addWidget(m_right);

    connect(closeButton,
            &QPushButton::clicked,
            this,
            [this]()
            {
                window()->close();
            });

    connect(minimizeButton,
            &QPushButton::clicked,
            this,
            [this]()
            {
                window()->showMinimized();
            });

    connect(maximizeButton,
            &QPushButton::clicked,
            this,
            [this]()
            {
                if (window()->isMaximized())
                    window()->showNormal();
                else
                    window()->showMaximized();
            });

    setStyleSheet(QString(R"(

#titleBar
{
    background:%1;
    border-top-left-radius:10px;
    border-top-right-radius:10px;
}

QLabel
{
    color:%2;
    font-size:13px;
    font-weight:bold;
    background:transparent;
}

)")
        .arg(Theme::panel().name())
        .arg(Theme::text().name()));
}

QWidget *TitleBar::leftContainer() const
{
    return m_left;
}

QWidget *TitleBar::rightContainer() const
{
    return m_right;
}

void TitleBar::setTitle(const QString &title)
{
    m_title->setText(title);
}

void TitleBar::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
        m_dragPos = event->globalPosition().toPoint()
                    - window()->frameGeometry().topLeft();
#else
        m_dragPos = event->globalPos()
                    - window()->frameGeometry().topLeft();
#endif
    }

    QWidget::mousePressEvent(event);
}

void TitleBar::mouseMoveEvent(QMouseEvent *event)
{
    if (!(event->buttons() & Qt::LeftButton))
    {
        QWidget::mouseMoveEvent(event);
        return;
    }

    if (window()->isMaximized())
        return;

    window()->move(event->globalPosition().toPoint() - m_dragPos);
    QWidget::mouseMoveEvent(event);
}

void TitleBar::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        if (window()->isMaximized())
            window()->showNormal();
        else
            window()->showMaximized();
    }

    QWidget::mouseDoubleClickEvent(event);
}