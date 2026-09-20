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

#include <QPaintEvent>
#include <QPainter>
#include <QMenu>
#include <QContextMenuEvent>
#include <QtDebug>
#include "buttons.h"
#include "vibro_widget.h"


VibroWidget::VibroWidget(QWidget *parent)
: QWidget{parent}
{
    QObject::connect(&m_timer, &QTimer::timeout, this, &VibroWidget::refresh);
    m_timer.start(30);
    QObject::connect(this, &QWidget::customContextMenuRequested, this, &VibroWidget::openContextMenu);
}

VibroWidget::~VibroWidget()
{
    m_timer.stop();
}

QSize VibroWidget::sizeHint() const
{
    return QSize(24, 24);
}

QSize VibroWidget::minimumSizeHint() const
{
    return QSize(24, 24);
}

void VibroWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    QRect r = this->rect();

    if(!m_vibroEnabled)
        p.fillRect(r, Qt::darkGray);
    else
        p.fillRect(r, Qt::white);

    p.setPen(Qt::transparent);

    p.save();
    p.setPen(QPen(Qt::black, 1));
    p.drawRect(r);
    p.restore();

    p.setPen(QPen(Qt::black, 1));
    p.drawLine(r.x(), r.center().y() + 1, r.right(), r.center().y() + 1);

    if(m_vibroEnabled &&
        (m_rumble_l[0] != 0 ||
         m_rumble_l[1] != 0 ||
         m_rumble_h[0] != 0 ||
         m_rumble_h[1] != 0))
    {
        QPoint prev;
        p.setPen(QPen(Qt::blue, 1));

        for(int i = 1; i < r.width(); ++i)
        {
            float rh = ((float)m_rumble_l[0] / 65535.0f);
            float rl = ((float)m_rumble_h[0] / 65535.0f);
            float y = SDL_sinf(i + m_iterator) * (rh * rl / 2.0f) * ((r.height() / 2)  - 2) + (r.height() / 2);
            QPoint cur(i, y);

            if(i == 1)
                prev = cur;

            p.drawLine(prev, cur);
            prev = cur;
        }

        p.setPen(QPen(Qt::red, 1));
        for(int i = 1; i < r.width(); ++i)
        {
            float rh = ((float)m_rumble_l[1] / 65535.0f);
            float rl = ((float)m_rumble_h[1] / 65535.0f);
            float y = SDL_sinf(i + m_iterator) * (rh * rl / 2.0f) * ((r.height() / 2)  - 2) + (r.height() / 2);
            QPoint cur(i, y);

            if(i == 1)
                prev = cur;

            p.drawLine(prev, cur);
            prev = cur;
        }
    }

    p.end();
}

void VibroWidget::contextMenuEvent(QContextMenuEvent *event)
{
    openContextMenu(event->pos());
}

void VibroWidget::openContextMenu(const QPoint &p)
{
    QMenu menu;

    QAction *v1 = menu.addAction("Quiet short vibro test");
    QAction *v2 = menu.addAction("Quiet long vibro test");

    QAction *v3 = menu.addAction("Loud short vibro test");
    QAction *v4 = menu.addAction("Loud long vibro test");

    QAction *v5 = menu.addAction("Both short vibro test");
    QAction *v6 = menu.addAction("Both long vibro test");

    QAction *reply = menu.exec(mapToGlobal(p));

    if(reply == v1)
        emit playTestRumble(10000, 0, 25);
    else if(reply == v2)
        emit playTestRumble(10000, 0, 1000);
    else if(reply == v3)
        emit playTestRumble(0, 65500, 25);
    else if(reply == v4)
        emit playTestRumble(0, 65500, 1000);
    else if(reply == v5)
        emit playTestRumble(10000, 65500, 25);
    else if(reply == v6)
        emit playTestRumble(10000, 65500, 1000);
}

void VibroWidget::playRumbleData(QByteArray data)
{
    if(!data.isEmpty())
    {
        nx_decodeRumble(data, 0, &m_rumble_l[0], &m_rumble_h[0]);
        nx_decodeRumble(data, 1, &m_rumble_l[1], &m_rumble_h[1]);

        if(m_rumble_l[0] != 0 || m_rumble_l[1] != 0 || m_rumble_h[0] != 0 || m_rumble_h[1] != 0)
            qDebug() << m_rumble_l[0] << m_rumble_h[0] << m_rumble_l[1] << m_rumble_h[1];

        repaint();
    }
}

void VibroWidget::setVibroEnabled(bool en)
{
    m_vibroEnabled = en;
    setContextMenuPolicy(Qt::CustomContextMenu);
    repaint();
}

void VibroWidget::refresh()
{
    if(m_vibroEnabled &&
        (m_rumble_l[0] != 0 ||
         m_rumble_l[1] != 0 ||
         m_rumble_h[0] != 0 ||
         m_rumble_h[1] != 0))
    {
        repaint();

        m_iterator += 0.3f;

        if(m_iterator >= 3.0f)
            m_iterator -= 3.0f;
    }
}
