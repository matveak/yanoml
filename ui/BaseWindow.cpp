#include "basewindow.h"
#include "windowframe.h"

#include <QVBoxLayout>

BaseWindow::BaseWindow(QWidget *parent)
	: QWidget(parent)
{
	setWindowFlags(
		Qt::FramelessWindowHint |
		Qt::Window);

	setAttribute(Qt::WA_TranslucentBackground);

	auto layout = new QVBoxLayout(this);

	layout->setSpacing(0);
	layout->setContentsMargins(8,8,8,8);

	m_frame = new WindowFrame(this);

	layout->addWidget(m_frame);
}

QWidget *BaseWindow::contentWidget() const
{
	return m_frame->contentWidget();
}

WindowFrame *BaseWindow::frame() const
{
	return m_frame;
}