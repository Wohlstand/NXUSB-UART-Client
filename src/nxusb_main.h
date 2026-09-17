/*
 * NXUSB UART Client
 *
 * Copyright (c) 2026-2026 Vitaly Novichkov
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef NXUSB_MAIN_H
#define NXUSB_MAIN_H

#include <QMainWindow>
#include <QMap>
#include <QSettings>

#include "nxserial.h"
#include "joy_handler.h"

QT_BEGIN_NAMESPACE
namespace Ui { class NXUsbMain; }
QT_END_NAMESPACE

class NXUsbMain : public QMainWindow
{
    Q_OBJECT

    QSettings m_settings;
    NXSerial m_serial;
    JoyHandler m_joy;

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

    void on_actionQuit_triggered();

    void on_actionAbout_triggered();

    void on_serialPort_editingFinished();

private:
    Ui::NXUsbMain *ui;
};
#endif // NXUSB_MAIN_H
