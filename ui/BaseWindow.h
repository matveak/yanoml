#pragma once

#include <QWidget>

class WindowFrame;

class BaseWindow : public QWidget
{
	Q_OBJECT

public:
	explicit BaseWindow(QWidget *parent = nullptr);

protected:
	QWidget *contentWidget() const;

	WindowFrame *frame() const;

private:
	WindowFrame *m_frame;
};