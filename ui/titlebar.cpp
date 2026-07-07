#include "titlebar.h"

#include "theme.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>

TitleBar::TitleBar(QWidget *window, QWidget *parent)
    : QWidget(parent),
      m_window(window)
{
    setObjectName("titleBar");
    setFixedHeight(36);

    auto mainLayout = new QHBoxLayout(this);

    mainLayout->setContentsMargins(8, 4, 8, 4);
    mainLayout->setSpacing(6);

    //==========================================================
    // Левая часть
    //==========================================================

    m_leftContainer = new QWidget(this);
    m_leftLayout = new QHBoxLayout(m_leftContainer);

    m_leftLayout->setContentsMargins(0, 0, 0, 0);
    m_leftLayout->setSpacing(4);

    //==========================================================
    // Заголовок
    //==========================================================

    m_title = new QLabel(this);

    m_title->setAlignment(Qt::AlignCenter);

    //==========================================================
    // Правая часть
    //==========================================================

    m_rightContainer = new QWidget(this);
    m_rightLayout = new QHBoxLayout(m_rightContainer);

    m_rightLayout->setContentsMargins(0, 0, 0, 0);
    m_rightLayout->setSpacing(2);

    auto minimizeButton = new QPushButton("—");
    auto maximizeButton = new QPushButton("□");
    auto closeButton = new QPushButton("✕");

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

        m_rightLayout->addWidget(button);
    }

    //==========================================================
    // Layout
    //==========================================================

    mainLayout->addWidget(m_leftContainer);

    mainLayout->addStretch();

    mainLayout->addWidget(m_title);

    mainLayout->addStretch();

    mainLayout->addWidget(m_rightContainer);

    //==========================================================
    // Кнопки
    //==========================================================

    connect(closeButton,
            &QPushButton::clicked,
            m_window,
            &QWidget::close);

    connect(minimizeButton,
            &QPushButton::clicked,
            m_window,
            &QWidget::showMinimized);

    connect(maximizeButton,
            &QPushButton::clicked,
            this,
            [this]
            {
                if (m_window->isMaximized())
                    m_window->showNormal();
                else
                    m_window->showMaximized();
            });

    //==========================================================
    // Style
    //==========================================================

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
    background:transparent;
    font-weight:bold;
}

QPushButton
{
    border:none;
    border-radius:6px;
    background:transparent;
    color:%2;
}

QPushButton:hover
{
    background:%3;
}

QPushButton:pressed
{
    background:%4;
}

)")
        .arg(Theme::panel().name())
        .arg(Theme::text().name())
        .arg(Theme::panelHighlight().name())
        .arg(Theme::panel().name()));

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
}

QWidget *TitleBar::leftContainer() const
{
    return m_leftContainer;
}

QWidget *TitleBar::rightContainer() const
{
    return m_rightContainer;
}

QHBoxLayout *TitleBar::leftLayout() const
{
    return m_leftLayout;
}

QHBoxLayout *TitleBar::rightLayout() const
{
    return m_rightLayout;
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
        m_dragPosition =
            event->globalPosition().toPoint() -
            m_window->frameGeometry().topLeft();
#else
        m_dragPosition =
            event->globalPos() -
            m_window->frameGeometry().topLeft();
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

    if (m_window->isMaximized())
        return;

#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
    m_window->move(event->globalPosition().toPoint() - m_dragPosition);
#else
    m_window->move(event->globalPos() - m_dragPosition);
#endif

    QWidget::mouseMoveEvent(event);
}

void TitleBar::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        if (m_window->isMaximized())
            m_window->showNormal();
        else
            m_window->showMaximized();
    }

    QWidget::mouseDoubleClickEvent(event);
}