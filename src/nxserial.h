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

#ifndef NXSERIAL_H
#define NXSERIAL_H

#include <QObject>
#include <QTimer>
#include <QtSerialPort/QSerialPort>

#include "buttons.h"

class NXSerial : public QObject
{
    Q_OBJECT

    QSerialPort m_serialPort;
    QByteArray m_readData;
    QTimer m_timer;

public:
    explicit NXSerial(QObject *parent = nullptr);

    void setBus(const QString &bus);

    bool isConnected() const;

    bool openBus();
    void closeBus();

    bool write(const NXSendCmd &data);

private slots:
    void handleReadyRead();
    void handleError(QSerialPort::SerialPortError error);

signals:
    void debugMessage(const QString &message);
    void errorMessage(const QString &message);
    void playerChange(int player);
    void imuEnabled(bool imuEn);
    void rumbleData(QByteArray data);
    void imuSensitivityData(const QByteArray &data);
    void imuRegWrite(const QByteArray &data);
    void homeLights(const QByteArray &data);
    void vibroEnabled(bool imuEn);
};

#endif // NXSERIAL_H
