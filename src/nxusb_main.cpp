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

#include <QTextBlock>
#include <QTextCursor>
#include <QCloseEvent>
#include <QMessageBox>
#include <QColorTransform>
#include <QtDebug>
#include <QIcon>

#include "nxusb_main.h"
#include "./ui_nxusb_main.h"

void NXUsbMain::closeEvent(QCloseEvent *event)
{
    if(m_serial.isConnected())
    {
        event->ignore();
        QMessageBox::information(this, tr("Still working!"), tr("You can't close the program until you will disconnect the thing!"), QMessageBox::Ok);
    }
}

NXUsbMain::NXUsbMain(QWidget *parent)
: QMainWindow(parent)
, ui(new Ui::NXUsbMain)
{
    ui->setupUi(this);

    qRegisterMetaType<NXButtonsState>("NXButtonsState");
    qRegisterMetaType<NXSendCmdQ>("NXSendCmdQ");

    m_settings.beginGroup("setup");
    ui->serialPort->setText(m_settings.value("serial-port", "/dev/ttyUSB0").toString());
    m_settings.endGroup();

    QIcon wIcon;
    wIcon.addFile(":/icon/icon16.png", QSize(16, 16));
    wIcon.addFile(":/icon/icon32.png", QSize(32, 32));
    wIcon.addFile(":/icon/icon48.png", QSize(48, 48));
    wIcon.addFile(":/icon/icon256.png", QSize(256, 256));
    setWindowIcon(wIcon);

    memset(&cmd, 0, sizeof(cmd));
    cmd.data.tail.tail[0] = 0xFF;
    cmd.tail = 0xFF;

    QObject::connect(&m_serial, &NXSerial::debugMessage, this, [this](const QString &msg)->void
    {
        logAppend("DEBUG: " + msg.trimmed().remove('\n').remove('\r'));
    });

    QObject::connect(&m_serial, &NXSerial::errorMessage, this, [this](const QString &msg)->void
    {
        logAppend("ERROR: " + msg.trimmed().remove('\n').remove('\r'));
    });

    QObject::connect(&m_serial, &NXSerial::imuEnabled, &m_joy, &JoyHandler::setIMU, Qt::QueuedConnection);
    QObject::connect(&m_serial, &NXSerial::imuEnabled, &m_joy, [this](bool en)->void
    {
        ui->stateIMU->setChecked(en);
    }, Qt::QueuedConnection);

    QObject::connect(&m_serial, &NXSerial::vibroEnabled, &m_joy, &JoyHandler::setVibro, Qt::QueuedConnection);
    QObject::connect(&m_serial, &NXSerial::vibroEnabled, &m_joy, [this](bool en)->void
    {
        ui->stateVibro->setChecked(en);
    }, Qt::QueuedConnection);

    QObject::connect(&m_serial, &NXSerial::rumbleData, &m_joy, &JoyHandler::playRunbleData, Qt::QueuedConnection);
    QObject::connect(&m_serial, &NXSerial::playerLedsChange, ui->ledIndicator, &NXLedsWidget::setState);

    QObject::connect(&m_joy, &JoyHandler::debugMessage, this, [this](const QString &msg)->void
    {
        logAppend("DEBUG: " + msg.trimmed().remove('\n').remove('\r'));
    }, Qt::QueuedConnection);

    QObject::connect(&m_joy, &JoyHandler::stateChanged, ui->gamepad, &GamepadWidget::updateState, Qt::QueuedConnection);
    QObject::connect(&m_joy, &JoyHandler::sendCommand, &m_serial, &NXSerial::write, Qt::BlockingQueuedConnection);
    QObject::connect(ui->gamepad, &GamepadWidget::sendCommand, &m_serial, &NXSerial::writeQ, Qt::QueuedConnection);

    QObject::connect(&m_joy, &JoyHandler::joysticksListUpdated, this, [this](void)->void
    {
        ui->joysticks->clear();
        ui->joysticks->addItems(m_joy.getJoysticks());
    }, Qt::QueuedConnection);
}

NXUsbMain::~NXUsbMain()
{
    delete ui;
}

void NXUsbMain::logAppend(const QString &msg)
{
    int maxLines = 200;

    ui->debugOut->appendPlainText(msg);

    int totalLines = ui->debugOut->document()->lineCount();

    if(totalLines > maxLines)
    {
        QString newText;

        for(int i = totalLines - maxLines; i < totalLines; ++i)
            newText += ui->debugOut->document()->findBlockByLineNumber(i).text() + (i == totalLines -1 ? "" : "\n");

        ui->debugOut->setPlainText(newText);
        QTextCursor c = ui->debugOut->textCursor();
        c.movePosition(QTextCursor::End);
        ui->debugOut->setTextCursor(c);
    }
}


void NXUsbMain::on_doConnect_clicked()
{
    if(m_serial.isConnected())
    {
        m_joy.stop();
        m_serial.closeBus();
        ui->doConnect->setText(tr("Connect"));
        ui->serialPort->setEnabled(true);
    }
    else
    {
        m_serial.setBus(ui->serialPort->text());
        if(m_serial.openBus())
        {
            ui->doConnect->setText(tr("Disconnect"));
            ui->serialPort->setEnabled(false);
            m_joy.start();

            on_querySetup_clicked();
        }
    }
}


void NXUsbMain::on_clearLog_clicked()
{
    ui->debugOut->clear();
}


void NXUsbMain::on_cmdReset_clicked()
{
    cmd.cmd = CMD_Reset;
    m_serial.write(cmd);
    ui->debugOut->appendPlainText(QString("Reset sent"));
}


void NXUsbMain::on_cmdPing_clicked()
{
    cmd.cmd = CMD_Ping;
    m_serial.write(cmd);
    ui->debugOut->appendPlainText(QString("Ping sent"));
}


void NXUsbMain::on_querySetup_clicked()
{
    cmd.cmd = CMD_QueryConfig;
    m_serial.write(cmd);
}


void NXUsbMain::on_joysticks_currentIndexChanged(int index)
{
    m_joy.selectActiveGamepad(index);
}

void NXUsbMain::on_actionQuit_triggered()
{
    this->close();
}


void NXUsbMain::on_actionAbout_triggered()
{
    QMessageBox::about(this, this->windowTitle(), tr("An UART client for the STM32-based Nintendo(r) Switch Pro Controller hardware emualtor.\n\nAuthor - Novichkov Vitaliy Dmitriyevich."));
}


void NXUsbMain::on_serialPort_editingFinished()
{
    m_settings.beginGroup("setup");
    m_settings.setValue("serial-port", ui->serialPort->text());
    m_settings.endGroup();

    m_settings.sync();
}

