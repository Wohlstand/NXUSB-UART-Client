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

#ifndef JOY_HANDLER_H
#define JOY_HANDLER_H

#include <QObject>
#include <QMutex>
#include <QMap>
#include <QAtomicInteger>
#include <QThread>

#include "buttons.h"

typedef int32_t SDL_JoystickID;

struct _SDL_Joystick;
typedef struct _SDL_Joystick SDL_Joystick;

struct _SDL_GameController;
typedef struct _SDL_GameController SDL_GameController;

struct SDL_Thread;
typedef struct SDL_Thread SDL_Thread;

class JoyHandler : public QObject
{
    Q_OBJECT

public:
    explicit JoyHandler(QObject *o = nullptr);
    virtual ~JoyHandler();

    void start();
    void stop();

    void setIMU(bool en);
    void setVibro(bool en);
    void playRunbleData(QByteArray data);

    QStringList getJoysticks();
    int numJoysticks();

    void selectActiveGamepad(int index);
    void playTestRumble(uint16_t low, uint16_t high, uint16_t duration);

signals:
    void debugMessage(const QString &msg);
    void sendCommand(const NXSendCmd &cmd);
    void stateChanged(NXButtonsState state);
    void joysticksListUpdated();
    void startThread();

private:
    void runner();
    static int runner_s(void *data);
    QAtomicInteger<bool> m_running;
    QAtomicInteger<bool> m_imuEnabled;
    QAtomicInteger<bool> m_vibroEnabled;
    QAtomicInteger<int> m_activeGamepad;

    QAtomicInteger<bool> m_testVibro;
    QAtomicInteger<uint16_t> m_testVibroLow;
    QAtomicInteger<uint16_t> m_testVibroHigh;
    QAtomicInteger<uint16_t> m_testVibroDuration;
    SDL_Thread *m_thread = nullptr;

    QByteArray m_rumble;
    QMutex m_rumbleMutex;

    NXButtonsState m_state;
    NXButtonsState m_statePrev;
    NXSendCmd m_buttons;
    NXSendCmd m_tilting;

    struct ControllerEntry
    {
        QString name;
        SDL_Joystick *joy = nullptr;
        SDL_GameController *ctrl = nullptr;
        bool swap_buttons = false;
        bool has_gyro = false;
        bool has_accel = false;
        bool has_rumble = false;
    };

    static void ctrlCleanUp(ControllerEntry &entry);

    void resetState();

    void addJoystick(int joyIdx);
    void removeJoystick(int instance);
    bool joystickProcess(ControllerEntry &entry, bool *buttons, bool *tilting);

    void joysticksSyncList();

    QMap<SDL_JoystickID, ControllerEntry> m_joysticks;
    QStringList m_joysticksNames;
    QMutex m_joysticksMutex;
};

#endif // JOY_HANDLER_H
