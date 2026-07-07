#pragma once

#include <QFrame>

class TitleBar;

class QVBoxLayout;

class WindowFrame : public QFrame
{
	Q_OBJECT

public:

	explicit WindowFrame(QWidget *parent = nullptr);

	QWidget *contentWidget() const;

	TitleBar *titleBar() const;

private:

	TitleBar *m_titleBar;

	QWidget *m_content;

	QVBoxLayout *m_layout;
};