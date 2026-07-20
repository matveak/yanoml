#include "windowframe.h"
#include "titlebar.h"
#include "theme.h"

#include <QVBoxLayout>

// ==================== Constructor ====================

WindowFrame::WindowFrame(QWidget *parent)
    : QFrame(parent)
{
    setupUi();
}

// ==================== UI Setup ====================

void WindowFrame::setupUi()
{
    setObjectName("windowFrame");

    m_layout = new QVBoxLayout(this);
    m_layout->setSpacing(0);
    m_layout->setContentsMargins(1, 1, 1, 1);

    // Заголовок окна (TitleBar)
    m_titleBar = new TitleBar(window(), this);
    m_layout->addWidget(m_titleBar);

    // Область для контента
    m_contentWidget = new QWidget(this);
    m_contentWidget->setObjectName("contentWidget");
    m_layout->addWidget(m_contentWidget);

    // ── Styles ─────────────────────────────────────────────────────────
    setStyleSheet(QString(R"(
        #windowFrame {
            background: %1;
            border: 1px solid %2;
            border-radius: 14px;
        }
        #titleBar {
            background: transparent;
            border: none;
            border-top-left-radius: 14px;
            border-top-right-radius: 14px;
        }
        #contentWidget {
            background: %1;
            border-bottom-left-radius: 14px;
            border-bottom-right-radius: 14px;
        }
    )")
    .arg(Theme::background().name())
    .arg(Theme::border().name()));
}

// ==================== Getters ====================

QWidget *WindowFrame::contentWidget() const
{
    return m_contentWidget;
}

TitleBar *WindowFrame::titleBar() const
{
    return m_titleBar;
}

// ==================== Public Methods ====================

void WindowFrame::setTitle(const QString &title)
{
    m_titleBar->setTitle(title);
}