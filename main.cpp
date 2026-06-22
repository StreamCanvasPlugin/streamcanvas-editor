#include "mainwindow.h"
#include "icons.h"

#include <QApplication>
#include <QPalette>

int main(int argc, char* argv[])
{
#if defined(QT_DEBUG) && defined(__linux__)
    qputenv("QT_LINUX_ACCESSIBILITY_ALWAYS_ON", "1");
#endif
    QApplication a(argc, argv);

    // Initialize SARibbon's embedded Qt resources (required for static linking).
    Q_INIT_RESOURCE(SARibbonResource);

    // ── Dark palette — colors keyed to SARibbon's dark2-default.json ──────────
    QApplication::setStyle("Fusion");

    const QColor bg    {0x19, 0x24, 0x30}; // accent / ribbon base
    const QColor panel {0x34, 0x42, 0x53}; // content-bg
    const QColor hover {0x4d, 0x59, 0x73}; // content-hover-bg
    const QColor text  {0xda, 0xda, 0xda}; // text-color
    const QColor dim   {0x5d, 0x63, 0x6a}; // text-disabled / separator
    const QColor blue  {0x2a, 0x8d, 0xb5}; // selection-bg / border-color

    QPalette dark;
    dark.setColor(QPalette::Window,          panel);
    dark.setColor(QPalette::WindowText,      text);
    dark.setColor(QPalette::Base,            bg);
    dark.setColor(QPalette::AlternateBase,   QColor{0x27, 0x33, 0x40});
    dark.setColor(QPalette::ToolTipBase,     panel);
    dark.setColor(QPalette::ToolTipText,     text);
    dark.setColor(QPalette::PlaceholderText, dim);
    dark.setColor(QPalette::Text,            text);
    dark.setColor(QPalette::Button,          panel);
    dark.setColor(QPalette::ButtonText,      text);
    dark.setColor(QPalette::BrightText,      Qt::white);
    dark.setColor(QPalette::Link,            blue);
    dark.setColor(QPalette::Highlight,       blue);
    dark.setColor(QPalette::HighlightedText, Qt::white);
    dark.setColor(QPalette::Mid,             hover);
    dark.setColor(QPalette::Dark,            bg);
    dark.setColor(QPalette::Shadow,          QColor{0x0a, 0x10, 0x18});

    dark.setColor(QPalette::Disabled, QPalette::Text,            dim);
    dark.setColor(QPalette::Disabled, QPalette::ButtonText,      dim);
    dark.setColor(QPalette::Disabled, QPalette::WindowText,      dim);
    dark.setColor(QPalette::Disabled, QPalette::Highlight,       QColor{0x1a, 0x3d, 0x4e});
    dark.setColor(QPalette::Disabled, QPalette::HighlightedText, dim);

    a.setPalette(dark);
    // ─────────────────────────────────────────────────────────────────────────

    initIcons();
    MainWindow w;
    w.show();
    return QApplication::exec();
}
