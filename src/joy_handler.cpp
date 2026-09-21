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

#include "joy_handler.h"

#include <SDL2/SDL_stdinc.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_joystick.h>
#include <SDL2/SDL_thread.h>
#include <SDL2/SDL_gamecontroller.h>

#include <QThread>
#include <QtDebug>


#define SWITCH_GYRO_SCALE_MULT 936.0
#define SWITCH_ACCEL_SCALE_MULT  4.0
#define SWITCH_GYRO_SCALE 14.2842
#define SWITCH_ACCEL_SCALE 4096.0

//#define SDL_STANDARD_GRAVITY 9.80665
#define GYRO_COEFF (M_PI / 180.0)

static constexpr float c_accelScale = SDL_STANDARD_GRAVITY / SWITCH_ACCEL_SCALE;
static constexpr float c_fAccelScaleX = c_accelScale;
static constexpr float c_fAccelScaleY = c_accelScale;
static constexpr float c_fAccelScaleZ = c_accelScale;

static constexpr float c_gyroScale = M_PI / 180.0 / SWITCH_GYRO_SCALE;
static constexpr float c_fGyroScaleX = c_gyroScale;
static constexpr float c_fGyroScaleY = c_gyroScale;
static constexpr float c_fGyroScaleZ = c_gyroScale;


JoyHandler::JoyHandler(QObject *o) :
    QObject(o)
{
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI, "1");
    SDL_Init(SDL_INIT_EVENTS | SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER | SDL_INIT_TIMER | SDL_INIT_HAPTIC | SDL_INIT_TIMER | SDL_INIT_SENSOR);

    resetState();
}

void JoyHandler::resetState()
{
    memset(&m_buttons, 0, sizeof(NXSendCmd));
    m_buttons.cmd = CMD_ButtonsUpdate;
    m_buttons.data.state.stick_l[0] = 0x7FF;
    m_buttons.data.state.stick_l[1] = 0x7FF;
    m_buttons.data.state.stick_r[0] = 0x7FF;
    m_buttons.data.state.stick_r[1] = 0x7FF;
    m_buttons.tail = 0xFF;

    memset(&m_tilting, 0, sizeof(NXSendCmd));
    m_tilting.cmd = CMD_Tilt;
    m_tilting.tail = 0xFF;

    memset(&m_state, 0, sizeof(NXButtonsState));
    memset(&m_statePrev, 0, sizeof(NXButtonsState));

    m_activeGamepad = 0;
    m_imuEnabled = false;
    m_vibroEnabled = false;
    m_sendRequest = false;
    m_vibroLevel = 100;
}

JoyHandler::~JoyHandler()
{
    stop();
    SDL_Quit();
}

void JoyHandler::start()
{
    stop();
    m_thread = SDL_CreateThread(&JoyHandler::runner_s, "Joystick Watcher", this);
    emit startThread();
}

void JoyHandler::stop()
{
    m_running = false;

    if(m_thread)
    {
        SDL_DetachThread(m_thread);
        m_thread = nullptr;
    }
}

void JoyHandler::setIMU(bool en)
{
    m_imuEnabled = en;
}

void JoyHandler::setVibro(bool en)
{
    m_vibroEnabled = en;
}

void JoyHandler::playRunbleData(QByteArray data)
{
    m_rumbleMutex.lock();
    m_rumble = data;
    m_rumbleMutex.unlock();
}

QStringList JoyHandler::getJoysticks()
{
    QStringList ret;

    m_joysticksMutex.lock();
    ret = m_joysticksNames;
    m_joysticksMutex.unlock();

    return ret;
}

int JoyHandler::numJoysticks()
{
    int ret;

    m_joysticksMutex.lock();
    ret = m_joysticksNames.size();
    m_joysticksMutex.unlock();

    return ret;
}

void JoyHandler::selectActiveGamepad(int index)
{
    m_activeGamepad = index;
}

void JoyHandler::playTestRumble(uint16_t low, uint16_t high, uint16_t duration)
{
    if(!m_running)
        return;

    m_testVibroLow = low;
    m_testVibroHigh = high;
    m_testVibroDuration = duration;
    m_testVibro = true;
}

void JoyHandler::requestSend()
{
    if(!m_running)
        return;

    m_sendRequest = true;
}

void JoyHandler::changeVibroLevel(int level)
{
    m_vibroLevel = (uint16_t)level;
}

void JoyHandler::runner()
{
    m_running = true;

    SDL_Event evt;
    Uint32 ticks_prev, ticks_cur;
    const Uint32 wait_delay = 1000 / 125;

    SDL_JoystickEventState(SDL_ENABLE);

    // Open already plugged joysticks
    int numJoysticks = SDL_NumJoysticks();

    for(int i = 0; i < numJoysticks; ++i)
        addJoystick(i);

    joysticksSyncList();

    // Start the watching
    while(m_running)
    {
        ticks_prev = SDL_GetTicks();

        while(SDL_PollEvent(&evt))
        {
            switch(evt.type)
            {
            case SDL_QUIT:
                m_running = false;
                break;

            case SDL_JOYDEVICEADDED:
                emit debugMessage(QString("-- Connecting new gamepad! Index %1").arg(evt.jdevice.which));
                addJoystick(evt.jdevice.which);
                joysticksSyncList();
                break;

            case SDL_JOYDEVICEREMOVED:
                emit debugMessage(QString("-- Disconnecting old gamepad! Instance %1").arg(evt.jdevice.which));
                removeJoystick(evt.jdevice.which);
                resetState();
                joysticksSyncList();
                break;
            }
        }

        int gamepad = 0;

        // Handle controls
        for(ControllerEntry &it : m_joysticks)
        {
            bool buttons, tilting;

            if(gamepad++ != m_activeGamepad)
                continue;

            if(joystickProcess(it, &buttons, &tilting))
            {
                if(buttons)
                    emit sendCommand(m_buttons);

                if(tilting)
                    emit sendCommand(m_tilting);
            }

            if(m_sendRequest)
            {
                emit sendCommand(m_buttons);
                emit sendCommand(m_tilting);
                m_sendRequest = false;
            }

            if(memcmp(&m_statePrev, &m_state, sizeof(NXButtonsState)) != 0)
                emit stateChanged(m_state);
        }

        ticks_cur = SDL_GetTicks();

        if(ticks_cur - ticks_prev < wait_delay)
            SDL_Delay(wait_delay - (ticks_cur - ticks_prev));
    }

    for(ControllerEntry &it : m_joysticks)
        ctrlCleanUp(it);

    m_joysticks.clear();

    resetState();

    emit stateChanged(m_state);
    emit sendCommand(m_buttons);
    emit sendCommand(m_tilting);

    joysticksSyncList();
}

int JoyHandler::runner_s(void *data)
{
    JoyHandler *self = reinterpret_cast<JoyHandler*>(data);
    self->runner();
    return 0;
}

uint16_t JoyHandler::applyVibroLevel(uint16_t vibro)
{
    return (uint16_t)((m_vibroLevel / 100.f) * vibro);
}

void JoyHandler::ctrlCleanUp(ControllerEntry &entry)
{
    if(entry.ctrl)
        SDL_GameControllerClose(entry.ctrl);

    if(entry.joy)
        SDL_JoystickClose(entry.joy);

    entry.ctrl = nullptr;
    entry.joy = nullptr;
}


void JoyHandler::addJoystick(int joyIdx)
{
    ControllerEntry entry;

    entry.joy = SDL_JoystickOpen(joyIdx);

    if(!entry.joy)
    {
        emit debugMessage(QString("Failed to open joystick: %1").arg(QString::fromUtf8(SDL_GetError())));
        return;
    }

    entry.name = QString::fromUtf8(SDL_JoystickName(entry.joy));
    emit debugMessage(QString("Opened joystick: %1").arg(entry.name));

    if(SDL_IsGameController(joyIdx))
    {
        entry.ctrl = SDL_GameControllerOpen(joyIdx);
        if(!entry.ctrl)
        {
            emit debugMessage(QString("Failed to open joystick as a game controller: %1").arg(QString::fromUtf8(SDL_GetError())));
            ctrlCleanUp(entry);
            return;
        }

        entry.swap_buttons = true;

        if(SDL_GameControllerGetType(entry.ctrl) == SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_PRO)
            entry.swap_buttons = false;

        if(SDL_GameControllerHasSensor(entry.ctrl, SDL_SENSOR_GYRO))
        {
            SDL_GameControllerSetSensorEnabled(entry.ctrl, SDL_SENSOR_GYRO, SDL_TRUE);
            entry.has_gyro = true;
        }

        if(SDL_GameControllerHasSensor(entry.ctrl, SDL_SENSOR_ACCEL))
        {
            SDL_GameControllerSetSensorEnabled(entry.ctrl, SDL_SENSOR_ACCEL, SDL_TRUE);
            entry.has_accel = true;
        }

        if(SDL_GameControllerHasRumble(entry.ctrl))
        {
            entry.has_rumble = true;
        }
    }

    SDL_JoystickID instance = SDL_JoystickInstanceID(entry.joy);

    m_joysticks.insert(instance, std::move(entry));
}

void JoyHandler::removeJoystick(int instance)
{
    auto it = m_joysticks.find(instance);

    if(it != m_joysticks.end())
    {
        ControllerEntry entry = it.value();
        ctrlCleanUp(entry);
        m_joysticks.remove(instance);
    }
}



static inline float processAxis(SDL_GameController *ctrl, SDL_GameControllerAxis axe)
{
    float x = (float)SDL_GameControllerGetAxis(ctrl, axe) / 32767.0f;

    // Clamp
    if(x > 1.0f)
        x = 1.0f;
    else if(x < -1.0f)
        x = -1.0f;

    return x;
}

static inline float filterDead(float in, float  x, float y)
{
    const float dead = 0.007f;

    if(SDL_fabsf(x) >= dead || SDL_fabsf(y) >= dead)
        return in;
    else if(SDL_fabsf(in) < dead)
        return 0.f;
    else
        return in;
}

#define setButton(dst, ctrl, key, mask) \
    if(SDL_GameControllerGetButton((ctrl), (key)) > 0) \
        (dst) |= (mask); \
    else \
        (dst) &= ~(mask);


#define setTrigger(dst, ctrl, key, mask) \
    if(SDL_GameControllerGetAxis((ctrl), (key)) > 500) \
        (dst) |= (mask); \
    else \
        (dst) &= ~(mask);



bool JoyHandler::joystickProcess(ControllerEntry &entry, bool *buttons, bool *tilting)
{
    uint16_t tmp[2], rumble_ll = 0, rumble_lh = 0, rumble_rl = 0, rumble_rh = 0;
    NXSendCmd bOld, tOld;
    memcpy(&bOld, &m_buttons, sizeof(NXSendCmd));
    memcpy(&tOld, &m_tilting, sizeof(NXSendCmd));
    memcpy(&m_statePrev, &m_state, sizeof(NXSendCmd));

    *buttons = false;
    *tilting = false;

    // Read buttons state

    float axis_l_x_val = processAxis(entry.ctrl, SDL_CONTROLLER_AXIS_LEFTX);
    float axis_l_y_val = -processAxis(entry.ctrl, SDL_CONTROLLER_AXIS_LEFTY);
    float axis_r_x_val = processAxis(entry.ctrl, SDL_CONTROLLER_AXIS_RIGHTX);
    float axis_r_y_val = -processAxis(entry.ctrl, SDL_CONTROLLER_AXIS_RIGHTY);

    m_state.stick_l[0] = axis_l_x_val = filterDead(axis_l_x_val, axis_l_x_val, axis_l_y_val);
    m_state.stick_l[1] = axis_l_y_val = filterDead(axis_l_y_val, axis_l_x_val, axis_l_y_val);
    m_state.stick_r[0] = axis_r_x_val = filterDead(axis_r_x_val, axis_r_x_val, axis_r_y_val);
    m_state.stick_r[1] = axis_r_y_val = filterDead(axis_r_y_val, axis_r_x_val, axis_r_y_val);

    nx_axis2bin(tmp, axis_l_x_val, axis_l_y_val, s_nx_calib_left);
    m_buttons.data.state.stick_l[0] = tmp[0];
    m_buttons.data.state.stick_l[1] = tmp[1];

    nx_axis2bin(tmp, axis_r_x_val, axis_r_y_val, s_nx_calib_right);
    m_buttons.data.state.stick_r[0] = tmp[0];
    m_buttons.data.state.stick_r[1] = tmp[1];

    setButton(m_buttons.data.state.buttons, entry.ctrl, SDL_CONTROLLER_BUTTON_LEFTSTICK, BUTTON_L_STICK_PRESS);
    setButton(m_buttons.data.state.buttons, entry.ctrl, SDL_CONTROLLER_BUTTON_RIGHTSTICK, BUTTON_R_STICK_PRESS);

    setButton(m_buttons.data.state.buttons, entry.ctrl, SDL_CONTROLLER_BUTTON_DPAD_LEFT, BUTTON_DPAD_LEFT);
    setButton(m_buttons.data.state.buttons, entry.ctrl, SDL_CONTROLLER_BUTTON_DPAD_RIGHT, BUTTON_DPAD_RIGHT);
    setButton(m_buttons.data.state.buttons, entry.ctrl, SDL_CONTROLLER_BUTTON_DPAD_UP, BUTTON_DPAD_UP);
    setButton(m_buttons.data.state.buttons, entry.ctrl, SDL_CONTROLLER_BUTTON_DPAD_DOWN, BUTTON_DPAD_DOWN);

    setButton(m_buttons.data.state.buttons, entry.ctrl, SDL_CONTROLLER_BUTTON_START, BUTTON_MINUS);
    setButton(m_buttons.data.state.buttons, entry.ctrl, SDL_CONTROLLER_BUTTON_BACK, BUTTON_PLUS);
    setButton(m_buttons.data.state.buttons, entry.ctrl, SDL_CONTROLLER_BUTTON_GUIDE, BUTTON_HOME);
    setButton(m_buttons.data.state.buttons, entry.ctrl, SDL_CONTROLLER_BUTTON_MISC1, BUTTON_CAPTURE);

    setButton(m_buttons.data.state.buttons, entry.ctrl, SDL_CONTROLLER_BUTTON_A, entry.swap_buttons ? BUTTON_B : BUTTON_A);
    setButton(m_buttons.data.state.buttons, entry.ctrl, SDL_CONTROLLER_BUTTON_B, entry.swap_buttons ? BUTTON_A : BUTTON_B);
    setButton(m_buttons.data.state.buttons, entry.ctrl, SDL_CONTROLLER_BUTTON_X, entry.swap_buttons ? BUTTON_Y : BUTTON_X);
    setButton(m_buttons.data.state.buttons, entry.ctrl, SDL_CONTROLLER_BUTTON_Y, entry.swap_buttons ? BUTTON_X : BUTTON_Y);

    setButton(m_buttons.data.state.buttons, entry.ctrl, SDL_CONTROLLER_BUTTON_LEFTSHOULDER, BUTTON_L);
    setButton(m_buttons.data.state.buttons, entry.ctrl, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, BUTTON_R);

    setTrigger(m_buttons.data.state.buttons, entry.ctrl, SDL_CONTROLLER_AXIS_TRIGGERLEFT, BUTTON_ZL);
    setTrigger(m_buttons.data.state.buttons, entry.ctrl, SDL_CONTROLLER_AXIS_TRIGGERRIGHT, BUTTON_ZR);

    if(memcmp(&m_buttons.data.state, &bOld.data.state, sizeof(m_buttons.data.state)) != 0)
        *buttons = true;

    m_state.buttons = m_buttons.data.state.buttons;

    if(m_imuEnabled)
    {
        // Read tilting state if available
        if(entry.has_gyro)
            SDL_GameControllerGetSensorData(entry.ctrl, SDL_SENSOR_GYRO, m_state.gyro, 3);
        else
            memset(m_state.gyro, 0, sizeof(float) * 3);

        if(entry.has_accel)
            SDL_GameControllerGetSensorData(entry.ctrl, SDL_SENSOR_ACCEL, m_state.accel, 3);
        else
            memset(m_state.accel, 0, sizeof(float) * 3);
    }
    else
    {
        memset(m_state.gyro, 0, sizeof(float) * 3);
        memset(m_state.accel, 0, sizeof(float) * 3);
    }

    m_tilting.data.tilt.gyro[0] = -(int16_t)(m_state.gyro[2] / c_fGyroScaleX);
    m_tilting.data.tilt.gyro[1] = -(int16_t)(m_state.gyro[0] / c_fGyroScaleY);
    m_tilting.data.tilt.gyro[2] = (int16_t)(m_state.gyro[1] / c_fGyroScaleZ);
    m_tilting.data.tilt.accel[0] = -(int16_t)(m_state.accel[2] / c_fAccelScaleX);
    m_tilting.data.tilt.accel[1] = -(int16_t)(m_state.accel[0] / c_fAccelScaleY);
    m_tilting.data.tilt.accel[2] = (int16_t)(m_state.accel[1] / c_fAccelScaleZ);

    if(memcmp(&m_tilting.data.tilt, &tOld.data.tilt, sizeof(m_tilting.data.tilt)) != 0)
        *tilting = true;

    // Run rumble
    m_rumbleMutex.lock();
    if(!m_rumble.isEmpty())
    {
        auto data = m_rumble;
        m_rumble.clear();

        nx_decodeRumble(data, 0, &rumble_ll, &rumble_lh);
        nx_decodeRumble(data, 1, &rumble_rl, &rumble_rh);
#ifdef DEBUG_BUILD
        if(rumble_ll || rumble_lh || rumble_rl || rumble_rh)
            qDebug() << "Rumble:" << rumble_ll << rumble_lh << rumble_rl << rumble_rh;
#endif

        SDL_GameControllerRumble(entry.ctrl,
                                 applyVibroLevel(SDL_max(rumble_ll, rumble_rl)),
                                 applyVibroLevel(SDL_max(rumble_lh, rumble_rh)),
                                 300);
    }
    m_rumbleMutex.unlock();

    if(m_testVibro)
    {
        SDL_GameControllerRumble(entry.ctrl,
                                 applyVibroLevel(m_testVibroLow),
                                 applyVibroLevel(m_testVibroHigh),
                                 m_testVibroDuration);
        m_testVibro = false;
    }

    return *tilting || *buttons;
}

void JoyHandler::joysticksSyncList()
{
    m_joysticksMutex.lock();
    m_joysticksNames.clear();
    for(ControllerEntry &it : m_joysticks)
        m_joysticksNames.push_back(it.name);
    m_joysticksMutex.unlock();

    emit joysticksListUpdated();
}
