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

#ifndef GAMEPAD_WIDGET_H
#define GAMEPAD_WIDGET_H

#include <QWidget>
#include <QHash>
#include <QPixmap>
#include "buttons.h"


class GamepadWidget : public QWidget
{
    Q_OBJECT

    NXButtonsState  m_state;
    NXSendCmd       m_mouseButtons;

    QPixmap         m_buttonA;
    QPixmap         m_buttonB;
    QPixmap         m_buttonX;
    QPixmap         m_buttonY;

    QPixmap         m_buttonL;
    QPixmap         m_buttonR;

    QPixmap         m_buttonZL;
    QPixmap         m_buttonZR;

    QPixmap         m_buttonPlus;
    QPixmap         m_buttonMinus;

    QPixmap         m_buttonHome;
    QPixmap         m_buttonCaputre;

    QPixmap         m_buttonDPad;
    QPixmap         m_buttonDPadL;
    QPixmap         m_buttonDPadR;
    QPixmap         m_buttonDPadU;
    QPixmap         m_buttonDPadD;
    QPixmap         m_buttonDPadLU;
    QPixmap         m_buttonDPadUR;
    QPixmap         m_buttonDPadRD;
    QPixmap         m_buttonDPadDL;

    QPixmap         m_buttonStickL;
    QPixmap         m_buttonStickLPress;
    QPixmap         m_buttonStickLL;
    QPixmap         m_buttonStickLR;
    QPixmap         m_buttonStickLU;
    QPixmap         m_buttonStickLD;
    QPixmap         m_buttonStickL_LU;
    QPixmap         m_buttonStickL_UR;
    QPixmap         m_buttonStickL_RD;
    QPixmap         m_buttonStickL_DU;

    QPixmap         m_buttonStickR;
    QPixmap         m_buttonStickRPress;
    QPixmap         m_buttonStickRL;
    QPixmap         m_buttonStickRR;
    QPixmap         m_buttonStickRU;
    QPixmap         m_buttonStickRD;
    QPixmap         m_buttonStickR_LU;
    QPixmap         m_buttonStickR_UR;
    QPixmap         m_buttonStickR_RD;
    QPixmap         m_buttonStickR_DU;

    QHash<QPoint, uint32_t> m_layout;

public:
    explicit GamepadWidget(QWidget *parent = nullptr);

private:
    void drawButton(QPoint offset, QPainter *p, uint32_t b);
    void drawDPad(QPoint offset, QPainter *p);
    void drawStick(QPoint offset, QPainter *p, float x, float y, uint32_t press);
    void drawCube(QPoint offset, QPainter *p, float *accel, float *gyro);

    bool collideButton(QPoint offset, QPoint mouse, uint32_t b);
    bool collideDPad(QPoint offset, QPoint mouse, uint32_t &b);
    bool collideStick(QPoint offset, QPoint mouse, float &x, float &y);

protected:
    void paintEvent(QPaintEvent *event) override;

    void mousePressEvent(QMouseEvent *evt) override;
    void mouseMoveEvent(QMouseEvent *evt) override;
    void mouseReleaseEvent(QMouseEvent *evt) override;

public slots:
    void updateState(NXButtonsState state);

signals:
    void sendCommand(const NXSendCmd &cmd);
};

#endif // GAMEPAD_WIDGET_H
