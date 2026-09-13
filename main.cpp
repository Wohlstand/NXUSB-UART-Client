#include "nxusb_main.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    NXUsbMain w;
    w.show();
    return QApplication::exec();
}
