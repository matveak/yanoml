#include "windowframe.h"
#include "titlebar.h"
#include "theme.h"

#include <QVBoxLayout>

WindowFrame::WindowFrame(QWidget *parent)
	: QFrame(parent)
{
	setObjectName("windowFrame");

	setStyleSheet(QString(R"(

QFrame#windowFrame
{
    background:%1;
    border:1px solid %2;
    border-radius:10px;
}

)")
		.arg(Theme::background().name())
		.arg(Theme::border().name()));

	m_layout = new QVBoxLayout(this);

	m_layout->setSpacing(0);
	m_layout->setContentsMargins(0,0,0,0);

	m_titleBar = new TitleBar(this);

	m_layout->addWidget(m_titleBar);

	m_content = new QWidget(this);

	m_layout->addWidget(m_content);
}

QWidget *WindowFrame::contentWidget() const
{
	return m_content;
}

TitleBar *WindowFrame::titleBar() const
{
	return m_titleBar;
}