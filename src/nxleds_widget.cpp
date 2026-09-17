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

#include <QPainter>
#include <QtDebug>

#include "nxleds_widget.h"


NXLedsWidget::NXLedsWidget(QWidget *parent) : QWidget{parent}
{
    QObject::connect(&m_timer, &QTimer::timeout, this, &NXLedsWidget::refresh);
    m_timer.start(250);
}

NXLedsWidget::~NXLedsWidget()
{
    m_timer.stop();
}

QSize NXLedsWidget::sizeHint() const
{
    return QSize(64, 16);
}

QSize NXLedsWidget::minimumSizeHint() const
{
    return QSize(64, 16);
}

void NXLedsWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    QRect r = this->rect();
    QRect ri(r.x() + 1, r.y() + 1, r.width() - 2, r.height() - 2);

    p.setPen(Qt::transparent);

    p.setBrush(Qt::gray);
    p.drawRect(r);

    p.setBrush(Qt::black);
    p.drawRect(ri);

    QRect led;

    for(size_t i = 0; i < 4; ++i)
    {
        led.setRect(ri.x() + (i * (ri.width() / 4)) + 1, r.y() + 2, (ri.width() / 4) - 2, ri.height() - 2);
        p.save();
        p.setBrush(m_light[i] ? Qt::darkGreen : Qt::black);
        p.drawRect(led);
        led.setRect(led.x() + 2, led.y() + 2, led.width() - 4, led.height() - 4);
        p.setBrush(m_light[i] ? Qt::green : Qt::darkGray);
        p.drawRect(led);
        p.restore();
    }

    p.end();
}

void NXLedsWidget::setState(uint8_t state)
{
    m_state = state;

    m_on[0] = m_state & 0x01 || m_state & 0x10;
    m_on[1] = m_state & 0x02 || m_state & 0x20;
    m_on[2] = m_state & 0x04 || m_state & 0x40;
    m_on[3] = m_state & 0x08 || m_state & 0x80;
    m_blink[0] = m_state & 0x10;
    m_blink[1] = m_state & 0x20;
    m_blink[2] = m_state & 0x40;
    m_blink[3] = m_state & 0x80;

    qDebug() << "Player state:" << QString::number(m_state, 16);

    repaint();
}

void NXLedsWidget::refresh()
{
    bool hasUpdate = false;

    for(size_t i = 0; i < 4; ++i)
    {
        bool o = m_light[i];

        if(!m_on[i])
            m_light[i] = false;
        else if(m_blink[i])
        {
            m_light[i] = m_blinkCounter[i] % 2;
            m_blinkCounter[i] = m_blinkCounter[i]++ % 2;
        }
        else
            m_light[i] = true;

        hasUpdate |= (o != m_light[i]);
    }

    if(hasUpdate)
        repaint();
}
