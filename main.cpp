#include "mainwindow.h"
#include "icons.h"

#include <QApplication>

int main(int argc, char* argv[])
{
#if defined(QT_DEBUG) && defined(__linux__)
    qputenv("QT_LINUX_ACCESSIBILITY_ALWAYS_ON", "1");
#endif
    QApplication a(argc, argv);
    initIcons();
    MainWindow w;
    w.show();
    return QApplication::exec();
}
