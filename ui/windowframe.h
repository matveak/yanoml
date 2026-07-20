#pragma once

#include <QFrame>

class QVBoxLayout;
class TitleBar;

class WindowFrame : public QFrame
{
	Q_OBJECT

public:
	explicit WindowFrame(QWidget *parent = nullptr);

	QWidget *contentWidget() const;

	TitleBar *titleBar() const;

	void setTitle(const QString& title);

private:
	void setupUi();

	QVBoxLayout *m_layout        = nullptr;
	TitleBar    *m_titleBar      = nullptr;
	QWidget     *m_contentWidget = nullptr;
};