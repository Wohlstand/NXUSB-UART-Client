#include <QTextBlock>
#include <QTextCursor>
#include <QCloseEvent>
#include <QMessageBox>

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

    memset(&buttons, 0, sizeof(buttons));
    buttons.cmd = CMD_ButtonsUpdate;
    buttons.data.state.stick_l[0] = 0x7FF;
    buttons.data.state.stick_l[1] = 0x7FF;
    buttons.data.state.stick_r[0] = 0x7FF;
    buttons.data.state.stick_r[1] = 0x7FF;
    buttons.tail = 0xFF;

    memset(&tilt, 0, sizeof(tilt));
    tilt.cmd = CMD_Tilt;
    tilt.tail = 0xFF;

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
    QObject::connect(&m_serial, &NXSerial::rumbleData, &m_joy, &JoyHandler::playRunbleData, Qt::QueuedConnection);

    QObject::connect(&m_serial, &NXSerial::playerChange, this, [this](int player)->void
    {
        ui->playerId->setText(QString("[%1][%2][%3][%4]")
            .arg(player >= 1 ? "*" : " ")
            .arg(player >= 2 ? "*" : " ")
            .arg(player >= 3 ? "*" : " ")
            .arg(player >= 4 ? "*" : " ")
        );
    });

    QObject::connect(&m_joy, &JoyHandler::debugMessage, this, [this](const QString &msg)->void
    {
        logAppend("DEBUG: " + msg.trimmed().remove('\n').remove('\r'));
    }, Qt::QueuedConnection);

    QObject::connect(&m_joy, &JoyHandler::sendCommand, &m_serial, &NXSerial::write, Qt::BlockingQueuedConnection);

    QObject::connect(&m_joy, &JoyHandler::joysticksListUpdated, this, [this](void)->void
    {
        ui->joysticks->clear();
        ui->joysticks->addItems(m_joy.getJoysticks());
    }, Qt::BlockingQueuedConnection);

    auto buildButton = [this](QToolButton *buttonObj, uint32_t new_btns, QString label)->void
    {
        QObject::connect(buttonObj, &QToolButton::pressed, this, [this, new_btns, label]()->void
        {
            buttons.data.state.buttons |= new_btns;
            m_serial.write(buttons);
            ui->debugOut->appendPlainText(QString("Press %1").arg(label));
        });

        QObject::connect(buttonObj, &QPushButton::released, this, [this, new_btns, label]()->void
        {
            buttons.data.state.buttons &= ~new_btns;
            m_serial.write(buttons);
            ui->debugOut->appendPlainText(QString("Release %1").arg(label));
        });
    };

    buildButton(ui->buttonsLpR, BUTTON_L | BUTTON_R, "L+R");

    buildButton(ui->buttonA, BUTTON_A, "A");
    buildButton(ui->buttonB, BUTTON_B, "B");
    buildButton(ui->buttonX, BUTTON_X, "X");
    buildButton(ui->buttonY, BUTTON_Y, "Y");

    buildButton(ui->buttonL, BUTTON_L, "L");
    buildButton(ui->buttonR, BUTTON_R, "R");
    buildButton(ui->buttonZL, BUTTON_ZL, "ZL");
    buildButton(ui->buttonZR, BUTTON_ZR, "ZR");

    buildButton(ui->buttonPlus, BUTTON_PLUS, "+");
    buildButton(ui->buttonMinus, BUTTON_MINUS, "-");

    buildButton(ui->buttonHome, BUTTON_HOME, "Home");
    buildButton(ui->buttonCapture, BUTTON_CAPTURE, "Capture");

    buildButton(ui->buttonDLeft, BUTTON_DPAD_LEFT, "D-Left");
    buildButton(ui->buttonDRight, BUTTON_DPAD_RIGHT, "D-Right");
    buildButton(ui->buttonDUp, BUTTON_DPAD_UP, "D-Up");
    buildButton(ui->buttonDDown, BUTTON_DPAD_DOWN, "D-Down");
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
        emit m_joy.joysticksListUpdated();
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
    cmd.cmd = CMD_QueryPlayer;
    m_serial.write(cmd);

    cmd.cmd = CMD_QueryIMU;
    m_serial.write(cmd);
}


void NXUsbMain::on_joysticks_currentIndexChanged(int index)
{
    m_joy.selectActiveGamepad(index);
}
