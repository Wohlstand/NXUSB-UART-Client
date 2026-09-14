#ifndef NXUSB_MAIN_H
#define NXUSB_MAIN_H

#include <QMainWindow>

#include "nxserial.h"
#include "joy_handler.h"

QT_BEGIN_NAMESPACE
namespace Ui { class NXUsbMain; }
QT_END_NAMESPACE

class NXUsbMain : public QMainWindow
{
    Q_OBJECT

    NXSerial m_serial;
    JoyHandler m_joy;

    NXSendCmd buttons;
    NXSendCmd tilt;
    NXSendCmd cmd;

protected:
    void closeEvent(QCloseEvent *event) override;

public:
    explicit NXUsbMain(QWidget *parent = nullptr);
    ~NXUsbMain() override;

    void logAppend(const QString &msg);

private slots:
    void on_doConnect_clicked();
    void on_clearLog_clicked();

    void on_cmdReset_clicked();

    void on_cmdPing_clicked();

    void on_querySetup_clicked();

    void on_joysticks_currentIndexChanged(int index);

private:
    Ui::NXUsbMain *ui;
};
#endif // NXUSB_MAIN_H
