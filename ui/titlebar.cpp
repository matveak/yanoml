#include "titlebar.h"
#include "theme.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>

// ==================== Constructor ====================

TitleBar::TitleBar(QWidget *window, QWidget *parent)
    : QWidget(parent),
      m_window(window)
{
    setObjectName("titleBar");
    setFixedHeight(36);

    auto *mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(8, 4, 8, 4);
    mainLayout->setSpacing(6);

    // ── Левая часть ────────────────────────────────────────────────────
    m_leftContainer = new QWidget(this);
    m_leftContainer->setObjectName("leftContainer");
    m_leftLayout = new QHBoxLayout(m_leftContainer);
    m_leftLayout->setContentsMargins(0, 0, 0, 0);
    m_leftLayout->setSpacing(4);

    // ── Заголовок ──────────────────────────────────────────────────────
    m_title = new QLabel(this);
    m_title->setAlignment(Qt::AlignCenter);

    // ── Правая часть (кнопки управления окном) ────────────────────────
    m_rightContainer = new QWidget(this);
    m_rightContainer->setObjectName("rightContainer");
    m_rightLayout = new QHBoxLayout(m_rightContainer);
    m_rightLayout->setContentsMargins(0, 0, 0, 0);
    m_rightLayout->setSpacing(2);

    auto *minimizeButton = new QPushButton("━");
    auto *maximizeButton = new QPushButton("□");
    auto *closeButton    = new QPushButton("✕");

    QList<QPushButton*> buttons = { minimizeButton, maximizeButton, closeButton };

    for (QPushButton *button : buttons) {
        button->setFixedSize(36, 30);  // Увеличили размер кнопок
        button->setCursor(Qt::PointingHandCursor);
        button->setFlat(true);

        // Делаем символы жирнее через стиль
        button->setStyleSheet(QString(
            "QPushButton { "
            "    border: none; "
            "    border-radius: 6px; "
            "    background: transparent; "
            "    color: %1; "
            "    font-weight: 900; "      // Максимальная жирность
            "    font-size: 16px; "       // Увеличенный размер шрифта
            "    line-height: 1; "        // Компактная высота строки
            "}"
            "QPushButton:hover { "
            "    background: %2; "
            "}"
            "QPushButton:pressed { "
            "    background: %3; "
            "}"
        ).arg(Theme::text().name(),
              Theme::panelHighlight().name(),
              Theme::panel().name()));

        m_rightLayout->addWidget(button);
    }

    // ── Layout ─────────────────────────────────────────────────────────
    mainLayout->addWidget(m_leftContainer);
    mainLayout->addStretch();
    mainLayout->addWidget(m_title);
    mainLayout->addStretch();
    mainLayout->addWidget(m_rightContainer);

    // ─ Connections ────────────────────────────────────────────────────
    connect(closeButton, &QPushButton::clicked, m_window, &QWidget::close);
    connect(minimizeButton, &QPushButton::clicked, m_window, &QWidget::showMinimized);

    connect(maximizeButton, &QPushButton::clicked, this, [this]() {
        if (m_window->isMaximized())
            m_window->showNormal();
        else
            m_window->showMaximized();
    });

    // ── Специальный стиль для кнопки закрытия (красный при наведении) ─
    closeButton->setStyleSheet(QString(
        "QPushButton { "
        "    border: none; "
        "    border-radius: 6px; "
        "    background: transparent; "
        "    color: %1; "
        "    font-weight: 900; "      // Максимальная жирность
        "    font-size: 16px; "       // Увеличенный размер
        "    line-height: 1; "
        "}"
        "QPushButton:hover { "
        "    background: #E81123; "
        "    color: white; "
        "}"
        "QPushButton:pressed { "
        "    background: #C50F1F; "
        "    color: white; "
        "}"
    ).arg(Theme::text().name()));

    // Альтернатива: можно использовать более толстые символы
    // minimizeButton->setText("━");  // более жирное тире
    // maximizeButton->setText("");  // более жирный квадрат
    // closeButton->setText("✖");     // более жирный крестик
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
    }
    QWidget::mouseDoubleClickEvent(event);
}