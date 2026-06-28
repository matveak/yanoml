#pragma once
#include <QString>

// Единая тёмная палитра для всех окон лаунчера
namespace DarkTheme {

inline const char* kBg      = "#16181C";
inline const char* kPanel   = "#26292F";
inline const char* kPanelHi = "#2F333A";
inline const char* kBorder  = "#3A3E45";
inline const char* kText    = "#E8EAED";
inline const char* kTextDim = "#9CA3AF";
inline const char* kAccent  = "#1BD96A";
inline const char* kAccentCF= "#F16436"; // CurseForge orange

// Применяется к любому QWidget/QDialog
inline QString dialogStyle()
{
    return QString(R"(
        QDialog, QWidget {
            background-color: #16181C;
            color: #E8EAED;
            font-family: "Segoe UI", Arial, sans-serif;
        }
        QLabel { color: #E8EAED; background: transparent; }
        QLineEdit {
            background-color: #26292F;
            border: 1px solid #3A3E45;
            border-radius: 8px;
            padding: 6px 12px;
            color: #E8EAED;
        }
        QLineEdit:focus { border-color: #1BD96A; }
        QTextBrowser {
            background-color: #26292F;
            border: 1px solid #3A3E45;
            border-radius: 10px;
            padding: 12px;
            color: #E8EAED;
        }
        QComboBox {
            background-color: #26292F;
            border: 1px solid #3A3E45;
            border-radius: 8px;
            padding: 6px 10px;
            color: #E8EAED;
            min-height: 28px;
        }
        QComboBox:hover { border-color: #1BD96A; }
        QComboBox::drop-down { border: none; }
        QComboBox QAbstractItemView {
            background-color: #26292F;
            color: #E8EAED;
            selection-background-color: #1BD96A;
            selection-color: #0A0A0A;
        }
        QPushButton {
            background-color: #26292F;
            color: #E8EAED;
            border: 1px solid #3A3E45;
            border-radius: 8px;
            padding: 8px 16px;
            font-size: 13px;
        }
        QPushButton:hover  { background-color: #3A3E45; border-color: #1BD96A; }
        QPushButton:pressed { background-color: #1BD96A; color: #0A0A0A; border-color: #1BD96A; }
        QProgressBar {
            background-color: #26292F;
            border: none;
            border-radius: 6px;
            height: 12px;
        }
        QProgressBar::chunk {
            background-color: #1BD96A;
            border-radius: 6px;
        }
        QScrollBar:vertical {
            background: transparent; width: 10px; margin: 0;
        }
        QScrollBar::handle:vertical {
            background: #3A3E45; border-radius: 5px; min-height: 30px;
        }
        QScrollBar::handle:vertical:hover { background: #4A4F57; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }
        QScrollArea { border: none; background: transparent; }
        QFrame#separator { background-color: #3A3E45; }
        QTabWidget::pane {
            border: 1px solid #3A3E45;
            border-radius: 8px;
            background-color: #16181C;
        }
        QTabBar::tab {
            background-color: #26292F;
            color: #9CA3AF;
            border: 1px solid #3A3E45;
            border-bottom: none;
            border-radius: 6px 6px 0 0;
            padding: 8px 20px;
            margin-right: 2px;
        }
        QTabBar::tab:selected {
            background-color: #16181C;
            color: #E8EAED;
            border-bottom: 2px solid #1BD96A;
        }
        QTabBar::tab:hover:!selected { background-color: #2F333A; }
    )");
}

// Стиль кнопки-акцента (зелёная)
inline QString accentButton()
{
    return QString(
        "QPushButton {"
        "  background-color: #1BD96A; color: #0A0A0A;"
        "  border: none; border-radius: 8px;"
        "  font-weight: bold; padding: 8px 16px;"
        "}"
        "QPushButton:hover  { background-color: #15C25E; }"
        "QPushButton:pressed { background-color: #0FA34E; }");
}

// Стиль кнопки CurseForge (оранжевая)
inline QString cfButton()
{
    return QString(
        "QPushButton {"
        "  background-color: #F16436; color: #fff;"
        "  border: none; border-radius: 8px;"
        "  font-weight: bold; padding: 8px 16px;"
        "}"
        "QPushButton:hover  { background-color: #d95a2d; }"
        "QPushButton:pressed { background-color: #c04f26; }");
}

} // namespace DarkTheme