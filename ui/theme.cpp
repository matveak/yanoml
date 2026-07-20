#include "theme.h"

#include <QPainter>
#include <QPixmap>
#include <QFont>
#include <QPen>

// ==================== Static Color Definitions ====================

QColor Theme::s_background(0x16, 0x18, 0x1C);
QColor Theme::s_panel(0x26, 0x29, 0x2F);
QColor Theme::s_panelHighlight(0x2F, 0x33, 0x3A);
QColor Theme::s_border(0x3A, 0x3E, 0x45);

QColor Theme::s_text(0xE8, 0xEA, 0xED);
QColor Theme::s_textDim(0x9C, 0xA3, 0xAF);

QColor Theme::s_accent(0x1B, 0xD9, 0x6A);
QColor Theme::s_accentCurseForge(0xF1, 0x64, 0x36);

// ==================== Getters ====================

const QColor& Theme::background()        { return s_background; }
const QColor& Theme::panel()             { return s_panel; }
const QColor& Theme::panelHighlight()    { return s_panelHighlight; }
const QColor& Theme::border()            { return s_border; }
const QColor& Theme::text()              { return s_text; }
const QColor& Theme::textDim()           { return s_textDim; }
const QColor& Theme::accent()            { return s_accent; }
const QColor& Theme::accentCurseForge()  { return s_accentCurseForge; }

// ==================== Setters ====================

void Theme::setBackground(const QColor& color)        { s_background = color; }
void Theme::setPanel(const QColor& color)             { s_panel = color; }
void Theme::setPanelHighlight(const QColor& color)    { s_panelHighlight = color; }
void Theme::setBorder(const QColor& color)            { s_border = color; }
void Theme::setText(const QColor& color)              { s_text = color; }
void Theme::setTextDim(const QColor& color)           { s_textDim = color; }
void Theme::setAccent(const QColor& color)            { s_accent = color; }
void Theme::setAccentCurseForge(const QColor& color)  { s_accentCurseForge = color; }

// ==================== Styles ====================

QString Theme::dialogStyle()
{
    return QString(R"(
        QDialog, QWidget {
            background-color: %1;
            color: %2;
            font-family: "Segoe UI", Arial, sans-serif;
        }
        QLabel {
            background: transparent;
            color: %2;
        }
        QLineEdit {
            background-color: %3;
            border: 1px solid %4;
            border-radius: 10px;
            padding: 6px 12px;
            color: %2;
        }
        QLineEdit:focus {
            border-color: %5;
        }
        QTextBrowser {
            background-color: %3;
            border: 1px solid %4;
            border-radius: 10px;
            padding: 12px;
            color: %2;
        }
        QComboBox {
            background-color: %3;
            border: 1px solid %4;
            border-radius: 10px;
            padding: 6px 10px;
            color: %2;
            min-height: 28px;
        }
        QComboBox:hover {
            border-color: %5;
        }
        QComboBox::drop-down {
            border: none;
        }
        QComboBox QAbstractItemView {
            background-color: %3;
            color: %2;
            selection-background-color: %5;
            selection-color: #0A0A0A;
        }
        QPushButton {
            background-color: %3;
            color: %2;
            border: 1px solid %4;
            border-radius: 10px;
            padding: 8px 16px;
            font-size: 13px;
        }
        QPushButton:hover {
            background-color: %6;
            border-color: %5;
        }
        QPushButton:pressed {
            background-color: %5;
            color: #0A0A0A;
            border-color: %5;
        }
        QProgressBar {
            background-color: %3;
            border: none;
            border-radius: 6px;
            height: 12px;
        }
        QProgressBar::chunk {
            background-color: %5;
            border-radius: 6px;
        }
        QScrollBar:vertical {
            background: transparent;
            width: 10px;
            margin: 0;
        }
        QScrollBar::handle:vertical {
            background: %4;
            border-radius: 5px;
            min-height: 30px;
        }
        QScrollBar::handle:vertical:hover {
            background: #4A4F57;
        }
        QScrollBar::add-line:vertical,
        QScrollBar::sub-line:vertical {
            height: 0;
        }
        QScrollBar::add-page:vertical,
        QScrollBar::sub-page:vertical {
            background: transparent;
        }
        QScrollArea {
            border: none;
            background: transparent;
        }
        QFrame#separator {
            background-color: %4;
        }
        QTabWidget::pane {
            border: 1px solid %4;
            border-radius: 12px;
            background-color: %1;
        }
        QTabBar::tab {
            background-color: %3;
            color: %7;
            border: 1px solid %4;
            border-bottom: none;
            border-radius: 6px 6px 0 0;
            padding: 8px 20px;
            margin-right: 2px;
        }
        QTabBar::tab:selected {
            background-color: %1;
            color: %2;
            border-bottom: 2px solid %5;
        }
        QTabBar::tab:hover:!selected {
            background-color: %6;
        }
    )")
    .arg(background().name())
    .arg(text().name())
    .arg(panel().name())
    .arg(border().name())
    .arg(accent().name())
    .arg(panelHighlight().name())
    .arg(textDim().name());
}

QString Theme::accentButtonStyle()
{
    return QString(R"(
        QPushButton {
            background-color: %1;
            color: #0A0A0A;
            border: none;
            border-radius: 10px;
            font-weight: bold;
            padding: 8px 16px;
        }
        QPushButton:hover {
            background-color: #15C25E;
        }
        QPushButton:pressed {
            background-color: #0FA34E;
        }
    )").arg(accent().name());
}

QString Theme::curseForgeButtonStyle()
{
    return QString(R"(
        QPushButton {
            background-color: %1;
            color: white;
            border: none;
            border-radius: 10px;
            font-weight: bold;
            padding: 8px 16px;
        }
        QPushButton:hover {
            background-color: #D95A2D;
        }
        QPushButton:pressed {
            background-color: #C04F26;
        }
    )").arg(accentCurseForge().name());
}

// ==================== Icons ====================

QIcon Theme::windowControlIcon(const QString& kind, const QColor& color)
{
    const int size = 16;
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    QPen pen(color);
    pen.setWidthF(1.4);
    pen.setCapStyle(Qt::RoundCap);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);

    if (kind == "minimize") {
        painter.drawLine(QPointF(4, 11), QPointF(12, 11));
    } else if (kind == "maximize") {
        painter.drawRoundedRect(QRectF(3.5, 3.5, 9, 9), 2, 2);
    } else if (kind == "restore") {
        painter.drawRoundedRect(QRectF(5.5, 3.5, 7, 7), 2, 2);
        painter.drawLine(QPointF(3.5, 5.5), QPointF(3.5, 11.5));
        painter.drawLine(QPointF(3.5, 11.5), QPointF(9.5, 11.5));
        painter.drawLine(QPointF(9.5, 11.5), QPointF(9.5, 10.5));
    } else if (kind == "close") {
        painter.drawLine(QPointF(4, 4), QPointF(12, 12));
        painter.drawLine(QPointF(12, 4), QPointF(4, 12));
    }

    return QIcon(pixmap);
}

QIcon Theme::platformIcon(const QColor& color, const QString& letter)
{
    const int size = 20;
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(color);
    painter.drawRoundedRect(0, 0, size, size, 6, 6);

    QFont font = painter.font();
    font.setBold(true);
    font.setPointSize(10);
    painter.setFont(font);
    painter.setPen(Qt::white);
    painter.drawText(QRect(0, 0, size, size), Qt::AlignCenter, letter);

    return QIcon(pixmap);
}