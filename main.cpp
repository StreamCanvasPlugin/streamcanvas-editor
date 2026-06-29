#include "mainwindow.h"
#include "icons.h"

#include <QApplication>
#include <QIcon>
#include <QPalette>

int main(int argc, char* argv[])
{
#if defined(QT_DEBUG) && defined(__linux__)
    qputenv("QT_LINUX_ACCESSIBILITY_ALWAYS_ON", "1");
#endif
    QApplication a(argc, argv);

    // Initialize SARibbon's embedded Qt resources (required for static linking).
    Q_INIT_RESOURCE(SARibbonResource);

    // ── Dark palette ──────────────────────────────────────────────────────────
    QApplication::setStyle("Fusion");

    const QColor bg     {0x19, 0x19, 0x19}; // base / input background
    const QColor panel  {0x2b, 0x2b, 0x2b}; // window / button surface
    const QColor hover  {0x3c, 0x3c, 0x3c}; // hover state
    const QColor text   {0xda, 0xda, 0xda}; // primary text
    const QColor dim    {0x68, 0x68, 0x68}; // disabled / placeholder
    const QColor accent {0x3d, 0x78, 0xb8}; // selection / highlight

    QPalette dark;
    dark.setColor(QPalette::Window,          panel);
    dark.setColor(QPalette::WindowText,      text);
    dark.setColor(QPalette::Base,            bg);
    dark.setColor(QPalette::AlternateBase,   QColor{0x22, 0x22, 0x22});
    dark.setColor(QPalette::ToolTipBase,     panel);
    dark.setColor(QPalette::ToolTipText,     text);
    dark.setColor(QPalette::PlaceholderText, dim);
    dark.setColor(QPalette::Text,            text);
    dark.setColor(QPalette::Button,          panel);
    dark.setColor(QPalette::ButtonText,      text);
    dark.setColor(QPalette::BrightText,      Qt::white);
    dark.setColor(QPalette::Link,            accent);
    dark.setColor(QPalette::Highlight,       accent);
    dark.setColor(QPalette::HighlightedText, Qt::white);
    dark.setColor(QPalette::Mid,             hover);
    dark.setColor(QPalette::Dark,            bg);
    dark.setColor(QPalette::Shadow,          QColor{0x0a, 0x0a, 0x0a});

    dark.setColor(QPalette::Disabled, QPalette::Text,            dim);
    dark.setColor(QPalette::Disabled, QPalette::ButtonText,      dim);
    dark.setColor(QPalette::Disabled, QPalette::WindowText,      dim);
    dark.setColor(QPalette::Disabled, QPalette::Highlight,       QColor{0x22, 0x44, 0x66});
    dark.setColor(QPalette::Disabled, QPalette::HighlightedText, dim);

    a.setPalette(dark);
    a.setStyleSheet(
        "QMainWindow::separator { width: 5px; height: 5px; background: #232323; }"
        "QMainWindow::separator:hover { background: #3c3c3c; }");
    // ─────────────────────────────────────────────────────────────────────────

    QApplication::setWindowIcon(QIcon(":/icons/app.png"));

    initIcons();
    MainWindow w;
    w.resize(1280, 720);
    w.show();
    return QApplication::exec();
}
