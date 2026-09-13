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
    void rumbleData(const QByteArray &data);
};

#endif // NXSERIAL_H
