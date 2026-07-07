#include "theme.h"

QColor Theme::s_background("#16181C");
QColor Theme::s_panel("#26292F");
QColor Theme::s_panelHighlight("#2F333A");
QColor Theme::s_border("#3A3E45");

QColor Theme::s_text("#E8EAED");
QColor Theme::s_textDim("#9CA3AF");

QColor Theme::s_accent("#1BD96A");
QColor Theme::s_accentCurseForge("#F16436");

//------------------------------------------------------------
// Цвета
//------------------------------------------------------------

const QColor& Theme::background()
{
    return s_background;
}

const QColor& Theme::panel()
{
    return s_panel;
}

const QColor& Theme::panelHighlight()
{
    return s_panelHighlight;
}

const QColor& Theme::border()
{
    return s_border;
}

const QColor& Theme::text()
{
    return s_text;
}

const QColor& Theme::textDim()
{
    return s_textDim;
}

const QColor& Theme::accent()
{
    return s_accent;
}

const QColor& Theme::accentCurseForge()
{
    return s_accentCurseForge;
}

//------------------------------------------------------------
// Setters
//------------------------------------------------------------

void Theme::setBackground(const QColor& color)
{
    s_background = color;
}

void Theme::setPanel(const QColor& color)
{
    s_panel = color;
}

void Theme::setPanelHighlight(const QColor& color)
{
    s_panelHighlight = color;
}

void Theme::setBorder(const QColor& color)
{
    s_border = color;
}

void Theme::setText(const QColor& color)
{
    s_text = color;
}

void Theme::setTextDim(const QColor& color)
{
    s_textDim = color;
}

void Theme::setAccent(const QColor& color)
{
    s_accent = color;
}

void Theme::setAccentCurseForge(const QColor& color)
{
    s_accentCurseForge = color;
}

//------------------------------------------------------------
// Стили
//------------------------------------------------------------

QString Theme::dialogStyle()
{
    return QString(R"(

QDialog, QWidget {
    background-color:%1;
    color:%2;
    font-family:"Segoe UI", Arial, sans-serif;
}

QLabel {
    background:transparent;
    color:%2;
}

QLineEdit {
    background-color:%3;
    border:1px solid %4;
    border-radius:8px;
    padding:6px 12px;
    color:%2;
}

QLineEdit:focus {
    border-color:%5;
}

QTextBrowser {
    background-color:%3;
    border:1px solid %4;
    border-radius:10px;
    padding:12px;
    color:%2;
}

QComboBox {
    background-color:%3;
    border:1px solid %4;
    border-radius:8px;
    padding:6px 10px;
    color:%2;
    min-height:28px;
}

QComboBox:hover {
    border-color:%5;
}

QComboBox::drop-down {
    border:none;
}

QComboBox QAbstractItemView {
    background-color:%3;
    color:%2;
    selection-background-color:%5;
    selection-color:#0A0A0A;
}

QPushButton {
    background-color:%3;
    color:%2;
    border:1px solid %4;
    border-radius:8px;
    padding:8px 16px;
    font-size:13px;
}

QPushButton:hover {
    background-color:%6;
    border-color:%5;
}

QPushButton:pressed {
    background-color:%5;
    color:#0A0A0A;
    border-color:%5;
}

QProgressBar {
    background-color:%3;
    border:none;
    border-radius:6px;
    height:12px;
}

QProgressBar::chunk {
    background-color:%5;
    border-radius:6px;
}

QScrollBar:vertical {
    background:transparent;
    width:10px;
    margin:0;
}

QScrollBar::handle:vertical {
    background:%4;
    border-radius:5px;
    min-height:30px;
}

QScrollBar::handle:vertical:hover {
    background:#4A4F57;
}

QScrollBar::add-line:vertical,
QScrollBar::sub-line:vertical {
    height:0;
}

QScrollBar::add-page:vertical,
QScrollBar::sub-page:vertical {
    background:transparent;
}

QScrollArea {
    border:none;
    background:transparent;
}

QFrame#separator {
    background-color:%4;
}

QTabWidget::pane {
    border:1px solid %4;
    border-radius:8px;
    background-color:%1;
}

QTabBar::tab {
    background-color:%3;
    color:%7;
    border:1px solid %4;
    border-bottom:none;
    border-radius:6px 6px 0 0;
    padding:8px 20px;
    margin-right:2px;
}

QTabBar::tab:selected {
    background-color:%1;
    color:%2;
    border-bottom:2px solid %5;
}

QTabBar::tab:hover:!selected {
    background-color:%6;
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

QPushButton
{
    background-color:%1;
    color:#0A0A0A;
    border:none;
    border-radius:8px;
    font-weight:bold;
    padding:8px 16px;
}

QPushButton:hover
{
    background-color:#15C25E;
}

QPushButton:pressed
{
    background-color:#0FA34E;
}

)")
        .arg(accent().name());
}

QString Theme::curseForgeButtonStyle()
{
    return QString(R"(

QPushButton
{
    background-color:%1;
    color:white;
    border:none;
    border-radius:8px;
    font-weight:bold;
    padding:8px 16px;
}

QPushButton:hover
{
    background-color:#D95A2D;
}

QPushButton:pressed
{
    background-color:#C04F26;
}

)")
        .arg(accentCurseForge().name());
}