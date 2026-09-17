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

#include <cmath>
#include <QPainter>
#include <QMouseEvent>
#include <QtDebug>

#include "gamepad_widget.h"

inline uint qHash(const QPoint& key, size_t seed = 0)
{
    return qHash(static_cast<qint64>(key.x()) << 32 | key.y(), seed);
}

GamepadWidget::GamepadWidget(QWidget *parent)
: QWidget{parent}
{
    memset(&m_state, 0, sizeof(NXButtonsState));

    memset(&m_mouseButtons, 0, sizeof(NXSendCmd));
    m_mouseButtons.cmd = CMD_ButtonsUpdate;
    m_mouseButtons.data.state.stick_l[0] = 0x7FF;
    m_mouseButtons.data.state.stick_l[1] = 0x7FF;
    m_mouseButtons.data.state.stick_r[0] = 0x7FF;
    m_mouseButtons.data.state.stick_r[1] = 0x7FF;
    m_mouseButtons.tail = 0xFF;

    m_buttonA.load(":/buttons/a.png");
    m_buttonB.load(":/buttons/b.png");
    m_buttonX.load(":/buttons/x.png");
    m_buttonY.load(":/buttons/y.png");
    m_buttonL.load(":/buttons/sh-l.png");
    m_buttonR.load(":/buttons/sh-r.png");
    m_buttonZL.load(":/buttons/tr-zl.png");
    m_buttonZR.load(":/buttons/tr-zr.png");
    m_buttonPlus.load(":/buttons/plus.png");
    m_buttonMinus.load(":/buttons/minus.png");
    m_buttonHome.load(":/buttons/home.png");
    m_buttonCaputre.load(":/buttons/capture.png");

    m_buttonDPad.load(":/buttons/dpad.png");
    m_buttonDPadL.load(":/buttons/dpad-left.png");
    m_buttonDPadR.load(":/buttons/dpad-right.png");
    m_buttonDPadU.load(":/buttons/dpad-up.png");
    m_buttonDPadD.load(":/buttons/dpad-down.png");
    m_buttonDPadLU.load(":/buttons/dpad-left-up.png");
    m_buttonDPadUR.load(":/buttons/dpad-up-right.png");
    m_buttonDPadRD.load(":/buttons/dpad-right-down.png");
    m_buttonDPadDL.load(":/buttons/dpad-down-left.png");

    m_buttonStickL.load(":/buttons/stick-l-neutral.png");
    m_buttonStickLPress.load(":/buttons/stick-l-press.png");
    m_buttonStickLL.load(":/buttons/stick-l-left.png");
    m_buttonStickLR.load(":/buttons/stick-l-right.png");
    m_buttonStickLU.load(":/buttons/stick-l-up.png");
    m_buttonStickLD.load(":/buttons/stick-l-down.png");
    m_buttonStickL_LU.load(":/buttons/stick-l-lu.png");
    m_buttonStickL_UR.load(":/buttons/stick-l-ur.png");
    m_buttonStickL_RD.load(":/buttons/stick-l-rd.png");
    m_buttonStickL_DU.load(":/buttons/stick-l-dl.png");

    m_buttonStickR.load(":/buttons/stick-r-neutral.png");
    m_buttonStickRPress.load(":/buttons/stick-r-press.png");
    m_buttonStickRL.load(":/buttons/stick-r-left.png");
    m_buttonStickRR.load(":/buttons/stick-r-right.png");
    m_buttonStickRU.load(":/buttons/stick-r-up.png");
    m_buttonStickRD.load(":/buttons/stick-r-down.png");
    m_buttonStickR_LU.load(":/buttons/stick-r-lu.png");
    m_buttonStickR_UR.load(":/buttons/stick-r-ur.png");
    m_buttonStickR_RD.load(":/buttons/stick-r-rd.png");
    m_buttonStickR_DU.load(":/buttons/stick-r-dl.png");

    /*
          0   1  2  3  4  5  6  7  8  9  10
       0  zl                             zr 0
       1  l                              r  1
       2            -           +           2
       3                              X     3
       4     LS        C     H     Y     A  4
       5                              B     5
       6        DPad            RS          6
          0   1  2  3  4  5  6  7  8  9 10
     */

    m_layout.insert(QPoint(10, 4), (uint32_t)BUTTON_A);
    m_layout.insert(QPoint(9, 5), (uint32_t)BUTTON_B);
    m_layout.insert(QPoint(9, 3), (uint32_t)BUTTON_X);
    m_layout.insert(QPoint(8, 4), (uint32_t)BUTTON_Y);

    m_layout.insert(QPoint(7, 2), (uint32_t)BUTTON_PLUS);
    m_layout.insert(QPoint(3, 2), (uint32_t)BUTTON_MINUS);

    m_layout.insert(QPoint(6, 4), (uint32_t)BUTTON_HOME);
    m_layout.insert(QPoint(4, 4), (uint32_t)BUTTON_CAPTURE);

    m_layout.insert(QPoint(5, 1), (uint32_t)BUTTON_PADDING); // Visualisation of gyroscope and accelerometer

    m_layout.insert(QPoint(0, 1), (uint32_t)BUTTON_L);
    m_layout.insert(QPoint(10, 1), (uint32_t)BUTTON_R);
    m_layout.insert(QPoint(0, 0), (uint32_t)BUTTON_ZL);
    m_layout.insert(QPoint(10, 0), (uint32_t)BUTTON_ZR);

    m_layout.insert(QPoint(1, 4), (uint32_t)BUTTON_L_STICK_PRESS);
    m_layout.insert(QPoint(7, 6), (uint32_t)BUTTON_R_STICK_PRESS);

    m_layout.insert(QPoint(2, 6), (uint32_t)BUTTON_DPAD_UP);
}

const int c_size_scale = 3;

void GamepadWidget::drawButton(QPoint offset, QPainter *p, uint32_t b)
{
    bool pressed = (m_state.buttons & b) != 0;
    int size_w = 10 * c_size_scale;
    int size_h = 10 * c_size_scale;

    QPixmap *pix = nullptr;

    switch(b)
    {
    default:
    case BUTTON_X:
        pix = &m_buttonX;
        break;

    case BUTTON_Y:
        pix = &m_buttonY;
        break;

    case BUTTON_A:
        pix = &m_buttonA;
        break;

    case BUTTON_B:
        pix = &m_buttonB;
        break;

    case BUTTON_L:
        pix = &m_buttonL;
        size_h = 8 * c_size_scale;
        size_w = size_h * pix->width() / pix->height();
        break;

    case BUTTON_R:
        pix = &m_buttonR;
        size_h = 8 * c_size_scale;
        size_w = size_h * pix->width() / pix->height();
        break;

    case BUTTON_ZL:
        pix = &m_buttonZL;
        size_h = 8 * c_size_scale;
        size_w = size_h * pix->width() / pix->height();
        break;

    case BUTTON_ZR:
        pix = &m_buttonZR;
        size_h = 8 * c_size_scale;
        size_w = size_h * pix->width() / pix->height();
        break;

    case BUTTON_MINUS:
        pix = &m_buttonMinus;
        break;

    case BUTTON_PLUS:
        pix = &m_buttonPlus;
        break;

    case BUTTON_HOME:
        pix = &m_buttonHome;
        break;

    case BUTTON_CAPTURE:
        pix = &m_buttonCaputre;
        break;
    }

    QRect dst(offset.x() - size_w / 2, offset.y() - size_h / 2, size_w, size_h);

    p->save();

    if(!pressed)
        p->setOpacity(0.5);

    p->drawPixmap(dst, *pix, pix->rect());
    p->restore();
}

void GamepadWidget::drawDPad(QPoint offset, QPainter *p)
{
    uint32_t mask = BUTTON_DPAD_LEFT | BUTTON_DPAD_RIGHT | BUTTON_DPAD_UP | BUTTON_DPAD_DOWN;
    uint32_t b = m_state.buttons & mask;
    const int size = 17 * c_size_scale;
    QRect dst(offset.x() - size / 2, offset.y() - size / 2, size, size);

    p->save();

    switch(b)
    {
    case BUTTON_DPAD_UP:
        p->drawPixmap(dst, m_buttonDPadU, m_buttonDPadU.rect());
        break;

    case BUTTON_DPAD_DOWN:
        p->drawPixmap(dst, m_buttonDPadD, m_buttonDPadD.rect());
        break;

    case BUTTON_DPAD_LEFT:
        p->drawPixmap(dst, m_buttonDPadL, m_buttonDPadL.rect());
        break;

    case BUTTON_DPAD_RIGHT:
        p->drawPixmap(dst, m_buttonDPadR, m_buttonDPadR.rect());
        break;

    case BUTTON_DPAD_UP | BUTTON_DPAD_LEFT:
        p->drawPixmap(dst, m_buttonDPadLU, m_buttonDPadLU.rect());
        break;

    case BUTTON_DPAD_UP | BUTTON_DPAD_RIGHT:
        p->drawPixmap(dst, m_buttonDPadUR, m_buttonDPadUR.rect());
        break;

    case BUTTON_DPAD_DOWN | BUTTON_DPAD_RIGHT:
        p->drawPixmap(dst, m_buttonDPadRD, m_buttonDPadRD.rect());
        break;

    case BUTTON_DPAD_DOWN | BUTTON_DPAD_LEFT:
        p->drawPixmap(dst, m_buttonDPadDL, m_buttonDPadDL.rect());
        break;

    default:
        p->setOpacity(0.5);
        p->drawPixmap(dst, m_buttonDPad, m_buttonDPad.rect());
        break;
    }

    p->restore();
}

void GamepadWidget::drawStick(QPoint offset, QPainter *p, float x, float y, uint32_t press)
{
    QPixmap *stick[] =
    {
        press == BUTTON_L_STICK_PRESS ? &m_buttonStickL : &m_buttonStickR,
        press == BUTTON_L_STICK_PRESS ? &m_buttonStickLPress : &m_buttonStickRPress,
        press == BUTTON_L_STICK_PRESS ? &m_buttonStickLL : &m_buttonStickRL,
        press == BUTTON_L_STICK_PRESS ? &m_buttonStickLR : &m_buttonStickRR,
        press == BUTTON_L_STICK_PRESS ? &m_buttonStickLU : &m_buttonStickRU,
        press == BUTTON_L_STICK_PRESS ? &m_buttonStickLD : &m_buttonStickRD,
        press == BUTTON_L_STICK_PRESS ? &m_buttonStickL_LU : &m_buttonStickR_LU,
        press == BUTTON_L_STICK_PRESS ? &m_buttonStickL_UR : &m_buttonStickR_UR,
        press == BUTTON_L_STICK_PRESS ? &m_buttonStickL_RD : &m_buttonStickR_RD,
        press == BUTTON_L_STICK_PRESS ? &m_buttonStickL_DU : &m_buttonStickR_DU,
    };

    enum stickDir
    {
        STICK_NEUTRAL = 0,
        STICK_PRESS,
        STICK_LEFT,
        STICK_RIGHT,
        STICK_UP,
        STICK_DOWN,
        STICK_LU,
        STICK_UR,
        STICK_RD,
        STICK_DL
    };

    const int size = 15 * c_size_scale;
    QRect dst(offset.x() - size / 2, offset.y() - size / 2, size, size);

    p->save();

    if(m_state.buttons & press)
        p->drawPixmap(dst, *stick[STICK_PRESS], stick[STICK_PRESS]->rect());
    else if(std::abs(x) < 0.5f && std::abs(y) < 0.5f)
    {
        p->setOpacity(0.5);
        p->drawPixmap(dst, *stick[STICK_NEUTRAL], stick[STICK_NEUTRAL]->rect());
    }
    else if(x >= 0.5f  && std::abs(y) < 0.5f)
        p->drawPixmap(dst, *stick[STICK_RIGHT], stick[STICK_RIGHT]->rect());
    else if(x <= -0.5f  && std::abs(y) < 0.5f)
        p->drawPixmap(dst, *stick[STICK_LEFT], stick[STICK_LEFT]->rect());
    else if(std::abs(x) < 0.5f && y >= 0.5f)
        p->drawPixmap(dst, *stick[STICK_UP], stick[STICK_UP]->rect());
    else if(std::abs(x) < 0.5f && y <= -0.5f)
        p->drawPixmap(dst, *stick[STICK_DOWN], stick[STICK_DOWN]->rect());
    else if(x < -0.5f && y > 0.5f)
        p->drawPixmap(dst, *stick[STICK_LU], stick[STICK_LU]->rect());
    else if(x > 0.5f && y > 0.5f)
        p->drawPixmap(dst, *stick[STICK_UR], stick[STICK_UR]->rect());
    else if(x > 0.5f && y < -0.5f)
        p->drawPixmap(dst, *stick[STICK_RD], stick[STICK_RD]->rect());
    else if(x < -0.5f && y < -0.5f)
        p->drawPixmap(dst, *stick[STICK_DL], stick[STICK_DL]->rect());

    p->setOpacity(1.0);
    p->setBrush(Qt::red);
    p->setPen(QPen(Qt::red, 6));
    p->drawPoint(offset.x() + 1 + (size / 2) * x, offset.y() + 1 + (size / 2) * -y);

    p->restore();
}

void GamepadWidget::drawCube(QPoint offset, QPainter *p, float *accel, float *gyro)
{
    struct CubeVertex
    {
        float x, y, z;
    };

    static const CubeVertex c_baseCube[8] =
    {
        {0.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f},
        {1.0f, 1.0f, 0.0f},
        {1.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 1.0f},
        {0.0f, 1.0f, 1.0f},
        {1.0f, 1.0f, 1.0f},
        {1.0f, 0.0f, 1.0f}
    };

    const int size = 15 * c_size_scale;
    QRect dst(offset.x() - size / 2, offset.y() - size / 2, size, size);
    int screenCube[8][3];

    const float D = 0.5f;
    const float OfsZ = 1.0f;
    const float accelScale = 0.01f;
    const float gyroScale = 0.1f;

    float accel_x = accel[0] * accelScale;
    float accel_y = accel[1] * accelScale;
    float accel_z = -accel[2] * accelScale;
    float gyro_x = gyro[0] * gyroScale;
    float gyro_y = gyro[1] * gyroScale;
    float gyro_z = gyro[2] * gyroScale;

    float sin_x = std::sin(gyro_x);
    float cos_x = std::cos(gyro_x);
    float sin_y = std::sin(gyro_y);
    float cos_y = std::cos(gyro_y);
    float sin_z = std::sin(gyro_z);
    float cos_z = std::cos(gyro_z);

    for(size_t v = 0; v < 8; ++v)
    {
        float x = c_baseCube[v].x - 0.5f;
        float y = c_baseCube[v].y - 0.5f;
        float z = c_baseCube[v].z - 0.5f;

        float rot_x = x * sin_x * sin_y * cos_z -
                      z * cos_x * sin_y * cos_z +
                      y * cos_x * sin_z +
                      z * sin_x * sin_z +
                      x * cos_y * cos_z;

        float rot_y = y * cos_x * cos_z +
                      z * sin_x * cos_z -
                      y * sin_x * sin_y * sin_z +
                      z * cos_x * sin_x * sin_z -
                      x * cos_y * sin_z;

        float rot_z = z * cos_x * cos_y -
                      y * sin_x * cos_y +
                      x * sin_y;

        rot_x += 0.5f;
        rot_y += 0.5f;
        rot_z += 0.5f;

        float z_off = rot_z + OfsZ;
        float x_proj = (rot_x - 0.5f + accel_x) * D / (1.0f + accel_z);
        float y_proj = (rot_y - 0.5f + accel_y) * D / (1.0f + accel_z);

        screenCube[v][0] = offset.x() + (x_proj * dst.width());
        screenCube[v][1] = offset.y() + (y_proj * dst.height());
        screenCube[v][2] = z_off;
    }

    p->save();
    p->setPen(QPen(Qt::black, 1));

    size_t edges[16][2] =
    {
        {0, 1},
        {1, 2},
        {2, 3},
        {3, 0},

        {0, 4},
        {1, 5},
        {2, 6},
        {3, 7},

        {4, 5},
        {5, 6},
        {6, 7},
        {7, 4}
    };

    for(size_t i = 0; i < 12; ++i)
    {
        p->drawLine(screenCube[edges[i][0]][0], screenCube[edges[i][0]][1],
                    screenCube[edges[i][1]][0], screenCube[edges[i][1]][1]);
    }

    p->restore();

}

bool GamepadWidget::collideButton(QPoint offset, QPoint mouse, uint32_t b)
{
    int size_w = 10 * c_size_scale;
    int size_h = 10 * c_size_scale;
    QRect dst(offset.x() - size_w / 2, offset.y() - size_h / 2, size_w, size_h);
    return dst.contains(mouse);
}

bool GamepadWidget::collideDPad(QPoint offset, QPoint mouse, uint32_t &b)
{
    const int size = 15 * c_size_scale;
    QRect dst(offset.x() - size / 2, offset.y() - size / 2, size, size);

    if(dst.contains(mouse))
    {
        if(mouse.x() > offset.x() + 10)
            b |= BUTTON_DPAD_RIGHT;
        else if(mouse.x() < offset.x() - 10)
            b |= BUTTON_DPAD_LEFT;

        if(mouse.y() > offset.y() + 10)
            b |= BUTTON_DPAD_DOWN;
        else if(mouse.y() < offset.y() - 10)
            b |= BUTTON_DPAD_UP;

        return true;
    }

    return false;
}

bool GamepadWidget::collideStick(QPoint offset, QPoint mouse, float &x, float &y)
{
    const int size = 15 * c_size_scale;
    QRect dst(offset.x() - size / 2, offset.y() - size / 2, size, size);

    if(dst.contains(mouse))
    {
        if(mouse.x() > offset.x() + 10)
            x = +1.0;
        else if(mouse.x() < offset.x() - 10)
            x = -1.0;

        if(mouse.y() > offset.y() + 10)
            y = -1.0;
        else if(mouse.y() < offset.y() - 10)
            y = +1.0;

        return true;
    }

    return false;
}

void GamepadWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    QRect r = this->rect();
    int padding = 20;
    r.setRect(r.x() + padding, r.y() + padding,
              r.width() - padding * 2, r.height() - padding * 2);

    for(auto it = m_layout.begin(); it != m_layout.end(); ++it)
    {
        QPoint cell = it.key();
        QPoint offset(r.x() + (cell.x() * r.width() / 10), r.y() + (cell.y() * r.height() / 7));

        switch(it.value())
        {
        case BUTTON_DPAD_UP:
            drawDPad(offset, &p);
            break;

        case BUTTON_L_STICK_PRESS:
            drawStick(offset, &p, m_state.stick_l[0], m_state.stick_l[1], it.value());
            break;

        case BUTTON_R_STICK_PRESS:
            drawStick(offset, &p, m_state.stick_r[0], m_state.stick_r[1], it.value());
            break;

        case BUTTON_PADDING:
            drawCube(offset, &p, m_state.accel, m_state.gyro);
            break;

        default:
            drawButton(offset, &p, it.value());
            break;
        }
    }

    p.end();
}

void GamepadWidget::mousePressEvent(QMouseEvent *evt)
{
    if(evt->button() != Qt::LeftButton)
    {
        QWidget::mousePressEvent(evt);
        return;
    }

    QRect r = this->rect();
    int padding = 20;
    r.setRect(r.x() + padding, r.y() + padding,
              r.width() - padding * 2, r.height() - padding * 2);

    for(auto it = m_layout.begin(); it != m_layout.end(); ++it)
    {
        QPoint cell = it.key();
        QPoint offset(r.x() + (cell.x() * r.width() / 10), r.y() + (cell.y() * r.height() / 7));

        switch(it.value())
        {
        case BUTTON_DPAD_UP:
        {
            uint32_t b = 0;
            if(collideDPad(offset, evt->pos(), b))
            {
                m_mouseButtons.data.state.buttons |= b;
                m_state.buttons |= b;
                emit sendCommand(NXSendCmdQ(m_mouseButtons));
                repaint();
            }
            break;
        }

        case BUTTON_L_STICK_PRESS:
        case BUTTON_R_STICK_PRESS:
        {
            uint16_t tmp[2];
            float x = 0.0f, y = 0.0f;

            if(collideStick(offset, evt->pos(), x, y))
            {
                if(std::abs(x) < 0.1f && std::abs(y) < 0.1f)
                {
                    m_mouseButtons.data.state.buttons |= it.value();
                    m_state.buttons |= it.value();
                }
                else
                {
                    if(it.value() == BUTTON_L_STICK_PRESS)
                    {
                        m_state.stick_l[0] = x;
                        m_state.stick_l[1] = y;

                        nx_axis2bin(tmp, x, y, s_nx_calib_left);
                        m_mouseButtons.data.state.stick_l[0] = tmp[0];
                        m_mouseButtons.data.state.stick_l[1] = tmp[1];
                    }
                    else
                    {
                        m_state.stick_r[0] = x;
                        m_state.stick_r[1] = y;

                        nx_axis2bin(tmp, x, y, s_nx_calib_right);
                        m_mouseButtons.data.state.stick_r[0] = tmp[0];
                        m_mouseButtons.data.state.stick_r[1] = tmp[1];
                    }
                }

                emit sendCommand(NXSendCmdQ(m_mouseButtons));
                repaint();
            }

            break;
        }

        case BUTTON_PADDING:
            if(collideButton(offset, evt->pos(), BUTTON_L | BUTTON_R))
            {
                m_mouseButtons.data.state.buttons |= BUTTON_L | BUTTON_R;
                m_state.buttons |= BUTTON_L | BUTTON_R;
                emit sendCommand(NXSendCmdQ(m_mouseButtons));
                repaint();
            }
            break;

        default:
            if(collideButton(offset, evt->pos(), it.value()))
            {
                m_mouseButtons.data.state.buttons |= it.value();
                m_state.buttons |= it.value();
                emit sendCommand(NXSendCmdQ(m_mouseButtons));
                repaint();
            }
            break;
        }
    }

    QWidget::mousePressEvent(evt);
}

void GamepadWidget::mouseMoveEvent(QMouseEvent *evt)
{
    QWidget::mouseMoveEvent(evt);
}

void GamepadWidget::mouseReleaseEvent(QMouseEvent *evt)
{
    if(evt->button() == Qt::LeftButton)
    {
        if(m_mouseButtons.data.state.buttons != 0 ||
           m_mouseButtons.data.state.stick_l[0] != 0x7FF ||
           m_mouseButtons.data.state.stick_l[1] != 0x7FF ||
           m_mouseButtons.data.state.stick_r[0] != 0x7FF ||
           m_mouseButtons.data.state.stick_r[1] != 0x7FF)
        {
            m_state.buttons = 0;
            m_state.stick_l[0] = 0.0f;
            m_state.stick_l[1] = 0.0f;
            m_state.stick_r[0] = 0.0f;
            m_state.stick_r[1] = 0.0f;
            m_mouseButtons.data.state.buttons = 0;
            m_mouseButtons.data.state.stick_l[0] = 0x7FF;
            m_mouseButtons.data.state.stick_l[1] = 0x7FF;
            m_mouseButtons.data.state.stick_r[0] = 0x7FF;
            m_mouseButtons.data.state.stick_r[1] = 0x7FF;
            emit sendCommand(NXSendCmdQ(m_mouseButtons));
            repaint();
        }
    }

    QWidget::mouseReleaseEvent(evt);
}

void GamepadWidget::updateState(NXButtonsState state)
{
    memcpy(&m_state, &state, sizeof(NXButtonsState));

    if(m_mouseButtons.data.state.buttons)
        m_state.buttons |= m_mouseButtons.data.state.buttons;

    if(m_mouseButtons.data.state.buttons != 0 ||
       m_mouseButtons.data.state.stick_l[0] != 0x7FF ||
       m_mouseButtons.data.state.stick_l[1] != 0x7FF ||
       m_mouseButtons.data.state.stick_r[0] != 0x7FF ||
       m_mouseButtons.data.state.stick_r[1] != 0x7FF)
    {
        m_state.stick_l[0] = ((float)m_mouseButtons.data.state.stick_l[0] - 0x7FF) / 0x7FF;
        m_state.stick_l[1] = ((float)m_mouseButtons.data.state.stick_l[1] - 0x7FF) / 0x7FF;
        m_state.stick_r[0] = ((float)m_mouseButtons.data.state.stick_r[0] - 0x7FF) / 0x7FF;
        m_state.stick_r[1] = ((float)m_mouseButtons.data.state.stick_r[1] - 0x7FF) / 0x7FF;
    }

    repaint();
}
