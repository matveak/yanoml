#pragma once

#include <QWidget>

class QLabel;

class QPushButton;

class QHBoxLayout;

class TitleBar : public QWidget
{
	Q_OBJECT

public:

	explicit TitleBar(QWidget *parent = nullptr);

	QWidget *leftContainer() const;

	QWidget *rightContainer() const;

	void setTitle(const QString&);

	signals:

		void minimizeRequested();

	void maximizeRequested();

	void closeRequested();

protected:

	void mousePressEvent(QMouseEvent *) override;

	void mouseMoveEvent(QMouseEvent *) override;

	void mouseDoubleClickEvent(QMouseEvent *) override;

private:

	QLabel *m_title;

	QWidget *m_left;

	QWidget *m_right;

	QPoint m_dragPos;
};