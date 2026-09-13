#include "nxserial.h"
#include <QtDebug>


NXSerial::NXSerial(QObject *parent)
: QObject{parent}
{
    m_serialPort.setBaudRate(QSerialPort::Baud115200);
    m_serialPort.setDataBits(QSerialPort::Data8);
    m_serialPort.setStopBits(QSerialPort::OneStop);
    m_serialPort.setParity(QSerialPort::NoParity);
    // m_serialPort.setReadBufferSize(3);

    QObject::connect(&m_serialPort, &QSerialPort::readyRead, this, &NXSerial::handleReadyRead);
    QObject::connect(&m_serialPort, &QSerialPort::errorOccurred, this, &NXSerial::handleError);
}

void NXSerial::setBus(const QString &bus)
{
    m_serialPort.setPortName(bus);
}

bool NXSerial::isConnected() const
{
    return m_serialPort.isOpen();
}

bool NXSerial::openBus()
{
    if(!m_serialPort.open(QIODevice::ReadWrite))
    {
        emit errorMessage(QObject::tr(
            "Failed to open port %1, error: %2")
            .arg(m_serialPort.portName())
            .arg(m_serialPort.errorString())
        );

        return false;
    }

    return true;
}

void NXSerial::closeBus()
{
    m_serialPort.close();
}

bool NXSerial::write(const NXSendCmd &data)
{
    if(!m_serialPort.isOpen())
        return false;

    size_t dataSize = sizeof(data);

    switch(data.cmd)
    {
    case CMD_Ping:
    case CMD_Reset:
    case CMD_QueryIMU:
    case CMD_QueryPlayer:
        dataSize = 2;
        break;
    }

    m_serialPort.write((const char*)&data, dataSize);
    m_serialPort.flush();

    // qDebug() << "Write: " << QByteArray((const char*)&data, sizeof(data)).toHex(',');
    return true;
}

void NXSerial::handleReadyRead()
{
    int tail;
    QByteArray inData = m_serialPort.readAll();

    m_readData.append(inData);

    // qDebug() << "TRACE: " << inData;

    while((tail = m_readData.indexOf(0x7F)) >= 0)
    {
        auto data = m_readData.mid(0, tail);
        m_readData.remove(0, tail + 1);

        if(data.size() > 0)
        {
            switch(data[0])
            {
            case 'd': // Debug Message
            {
                QString debug = QString::fromUtf8(data.mid(1, -1));
                qDebug() << "Debug: " << debug;
                emit debugMessage(debug);
                break;
            }

            case 'p': // Player Number
            {
                if(data.size() < 2)
                {
                    emit errorMessage(QObject::tr("Received invalid data for Player number: %1").arg(QString::fromUtf8(data.toHex(','))));
                    break; // Invalid data
                }

                qDebug() << "Change player" << (int)data[1];

                playerChange(data[1]);
                break;
            }

            case 'r': // Rumble data
            {
                qDebug() << "Rumble data" << QString::fromUtf8(data.toHex(','));
                emit rumbleData(data.mid(1, -1));
                break;
            }

            case 'i': // IMU enable/disable
            {
                if(data.size() < 2)
                {
                    emit errorMessage(QObject::tr("Received invalid IMU enable flag: %1").arg(QString::fromUtf8(data.toHex(','))));
                    break; // Invalid data
                }

                qDebug() << "IMU toggled" << (int)data[1];
                emit imuEnabled(data[1] != '\0');
                break;
            }
            }
        }
    }

    if(m_readData.size() >= 4094)
    {
        qDebug() << "Garbage dropped: " << m_readData;
        m_readData.clear(); // Too much of junk!
    }
}

void NXSerial::handleError(QSerialPort::SerialPortError serialPortError)
{
    if(serialPortError == QSerialPort::ReadError)
    {
        emit errorMessage(QObject::tr(
            "An I/O error occurred while reading "
            "the data from port %1, error: %2")
            .arg(m_serialPort.portName())
            .arg(m_serialPort.errorString())
        );
    }
}
