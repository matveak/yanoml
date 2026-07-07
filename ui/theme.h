#pragma once

#include <QColor>
#include <QString>

class Theme
{
public:
	// ===== Основные цвета =====

	static const QColor& background();
	static const QColor& panel();
	static const QColor& panelHighlight();
	static const QColor& border();

	static const QColor& text();
	static const QColor& textDim();

	static const QColor& accent();
	static const QColor& accentCurseForge();

	// ===== Стили =====

	static QString dialogStyle();

	static QString accentButtonStyle();

	static QString curseForgeButtonStyle();

	// ===== Изменение темы =====

	static void setBackground(const QColor& color);
	static void setPanel(const QColor& color);
	static void setPanelHighlight(const QColor& color);
	static void setBorder(const QColor& color);

	static void setText(const QColor& color);
	static void setTextDim(const QColor& color);

	static void setAccent(const QColor& color);
	static void setAccentCurseForge(const QColor& color);

private:
	Theme() = delete;

	static QColor s_background;
	static QColor s_panel;
	static QColor s_panelHighlight;
	static QColor s_border;

	static QColor s_text;
	static QColor s_textDim;

	static QColor s_accent;
	static QColor s_accentCurseForge;
};