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

#ifndef NXLEDS_WIDGET_H
#define NXLEDS_WIDGET_H

#include <QWidget>
#include <QTimer>

class NXLedsWidget : public QWidget
{
    Q_OBJECT

    uint8_t m_state = 0;
    QTimer m_timer;

    bool m_on[4] = {false, false, false, false};
    bool m_light[4] = {false, false, false, false};
    bool m_blink[4] = {false, false, false, false};
    uint8_t m_blinkCounter[4] = {0, 0, 0, 0};

public:
    explicit NXLedsWidget(QWidget *parent = nullptr);
    virtual ~NXLedsWidget();

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

public slots:
    void setState(uint8_t state);

private slots:
    void refresh();
};

#endif // NXLEDS_WIDGET_H
