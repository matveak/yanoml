#include "windowframe.h"

#include "titlebar.h"
#include "theme.h"

#include <QVBoxLayout>

WindowFrame::WindowFrame(QWidget *parent)
	: QFrame(parent)
{
	setupUi();
}

void WindowFrame::setupUi()
{
	setObjectName("windowFrame");

	m_layout = new QVBoxLayout(this);

	m_layout->setSpacing(0);
	m_layout->setContentsMargins(1, 1, 1, 1);

	m_titleBar = new TitleBar(window(), this);

	m_layout->addWidget(m_titleBar);

	m_contentWidget = new QWidget(this);
	m_contentWidget->setObjectName("contentWidget");

	m_layout->addWidget(m_contentWidget);

	setStyleSheet(QString(R"(

#windowFrame
{
    background:%1;
    border:1px solid %2;
    border-radius:10px;
}

#contentWidget
{
    background:%1;
    border-bottom-left-radius:10px;
    border-bottom-right-radius:10px;
}

)")
		.arg(Theme::background().name())
		.arg(Theme::border().name()));
}

QWidget *WindowFrame::contentWidget() const
{
	return m_contentWidget;
}

TitleBar *WindowFrame::titleBar() const
{
	return m_titleBar;
}

void WindowFrame::setTitle(const QString &title)
{
	m_titleBar->setTitle(title);
}