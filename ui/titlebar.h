#pragma once

#include <QPoint>
#include <QWidget>

class QLabel;
class QPushButton;
class QHBoxLayout;

class TitleBar : public QWidget
{
	Q_OBJECT

public:
	explicit TitleBar(QWidget* window, QWidget* parent = nullptr);

	void setTitle(const QString& title);

	QWidget* leftContainer() const;
	QWidget* rightContainer() const;

	QHBoxLayout* leftLayout() const;
	QHBoxLayout* rightLayout() const;

protected:
	void mousePressEvent(QMouseEvent* event) override;
	void mouseMoveEvent(QMouseEvent* event) override;
	void mouseDoubleClickEvent(QMouseEvent* event) override;

private:
	QWidget* m_window;

	QLabel* m_title;

	QWidget* m_leftContainer;
	QWidget* m_rightContainer;

	QHBoxLayout* m_leftLayout;
	QHBoxLayout* m_rightLayout;

	QPoint m_dragPosition;
};