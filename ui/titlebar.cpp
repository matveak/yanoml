#include "titlebar.h"
#include "theme.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QSize>
#include <QColor>

// ==================== Constructor ====================

TitleBar::TitleBar(QWidget *window, QWidget *parent)
    : QWidget(parent),
      m_window(window)
{
    setObjectName("titleBar");
    setFixedHeight(32);
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet(QString(
        "#titleBar {"
        "    background: transparent;"
        "    border: none;"
        "    border-top-left-radius: 14px;"
        "    border-top-right-radius: 14px;"
        "    border-bottom: 1px solid rgba(255, 255, 255, 14);"
        "}"
    ));

    auto *mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(10, 0, 6, 0);
    mainLayout->setSpacing(8);

    // ── Левая часть (иконка приложения / доп. виджеты) ────────────────────
    m_leftContainer = new QWidget(this);
    m_leftContainer->setObjectName("leftContainer");
    m_leftLayout = new QHBoxLayout(m_leftContainer);
    m_leftLayout->setContentsMargins(0, 0, 0, 0);
    m_leftLayout->setSpacing(6);

    auto *appIcon = new QLabel(this);
    appIcon->setFixedSize(18, 18);
    appIcon->setAlignment(Qt::AlignCenter);
    appIcon->setPixmap(Theme::platformIcon(QColor("#8B5CF6"), "Y").pixmap(18, 18));
    m_leftLayout->addWidget(appIcon);

    // ── Заголовок ──────────────────────────────────────────────────────
    m_title = new QLabel(this);
    m_title->setObjectName("titleBarLabel");
    m_title->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    m_title->setStyleSheet(QString(
        "QLabel#titleBarLabel {"
        "    background: transparent;"
        "    color: %1;"
        "    font-size: 12px;"
        "    font-weight: 600;"
        "    letter-spacing: 0.2px;"
        "}"
    ).arg(Theme::textDim().name()));

    // ── Правая часть (кнопки управления окном) ────────────────────────
    m_rightContainer = new QWidget(this);
    m_rightContainer->setObjectName("rightContainer");
    m_rightLayout = new QHBoxLayout(m_rightContainer);
    m_rightLayout->setContentsMargins(0, 0, 0, 0);
    m_rightLayout->setSpacing(2);

    auto *minimizeButton = new QPushButton();
    m_maximizeButton      = new QPushButton();
    auto *closeButton    = new QPushButton();

    QList<QPushButton*> buttons = { minimizeButton, m_maximizeButton, closeButton };

    const auto baseStyle = QString(
        "QPushButton { "
        "    border: none; "
        "    border-radius: 6px; "
        "    background: transparent; "
        "}"
        "QPushButton:hover { "
        "    background: rgba(255, 255, 255, 26); "
        "}"
        "QPushButton:pressed { "
        "    background: rgba(255, 255, 255, 42); "
        "}"
    );

    for (QPushButton *button : buttons) {
        button->setFixedSize(30, 24);
        button->setIconSize(QSize(12, 12));
        button->setCursor(Qt::PointingHandCursor);
        button->setFlat(true);
        button->setStyleSheet(baseStyle);
        m_rightLayout->addWidget(button);
    }

    minimizeButton->setIcon(Theme::windowControlIcon("minimize", Theme::text()));

    // ── Layout ─────────────────────────────────────────────────────────
    mainLayout->addWidget(m_leftContainer);
    mainLayout->addWidget(m_title, 1);
    mainLayout->addWidget(m_rightContainer);

    // ─ Connections ────────────────────────────────────────────────────
    connect(closeButton, &QPushButton::clicked, m_window, &QWidget::close);
    connect(minimizeButton, &QPushButton::clicked, m_window, &QWidget::showMinimized);

    connect(m_maximizeButton, &QPushButton::clicked, this, [this]() {
        if (m_window->isMaximized())
            m_window->showNormal();
        else
            m_window->showMaximized();
        updateMaximizeIcon();
    });

    // ── Специальный стиль для кнопки закрытия (красный при наведении) ─
    closeButton->setIcon(Theme::windowControlIcon("close", Theme::text()));
    closeButton->setStyleSheet(
        "QPushButton { "
        "    border: none; "
        "    border-radius: 6px; "
        "    background: transparent; "
        "}"
        "QPushButton:hover { "
        "    background: #E81123; "
        "}"
        "QPushButton:pressed { "
        "    background: #C50F1F; "
        "}"
    );

    updateMaximizeIcon();
}

// ==================== Getters ====================

QWidget *TitleBar::leftContainer() const    { return m_leftContainer; }
QWidget *TitleBar::rightContainer() const   { return m_rightContainer; }
QHBoxLayout *TitleBar::leftLayout() const   { return m_leftLayout; }
QHBoxLayout *TitleBar::rightLayout() const  { return m_rightLayout; }

// ==================== Public Methods ====================

void TitleBar::setTitle(const QString &title)
{
    m_title->setText(title);
}

// ==================== Private Methods ====================

void TitleBar::updateMaximizeIcon()
{
    if (!m_maximizeButton)
        return;

    const QString kind = m_window->isMaximized() ? "restore" : "maximize";
    m_maximizeButton->setIcon(Theme::windowControlIcon(kind, Theme::text()));
}

// ==================== Event Handlers ====================

void TitleBar::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        m_dragPosition = event->globalPosition().toPoint() - m_window->frameGeometry().topLeft();
#else
        m_dragPosition = event->globalPos() - m_window->frameGeometry().topLeft();
#endif
    }
    QWidget::mousePressEvent(event);
}

void TitleBar::mouseMoveEvent(QMouseEvent *event)
{
    if (!(event->buttons() & Qt::LeftButton) || m_window->isMaximized()) {
        QWidget::mouseMoveEvent(event);
        return;
    }

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    m_window->move(event->globalPosition().toPoint() - m_dragPosition);
#else
    m_window->move(event->globalPos() - m_dragPosition);
#endif

    QWidget::mouseMoveEvent(event);
}

void TitleBar::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        if (m_window->isMaximized())
            m_window->showNormal();
        else
            m_window->showMaximized();
        updateMaximizeIcon();
    }
    QWidget::mouseDoubleClickEvent(event);
}
