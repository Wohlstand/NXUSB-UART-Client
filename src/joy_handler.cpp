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


JoyHandler::JoyHandler(QObject *o) :
    QObject(o)
{
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    SDL_Init(SDL_INIT_EVENTS | SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER | SDL_INIT_TIMER | SDL_INIT_HAPTIC | SDL_INIT_TIMER | SDL_INIT_SENSOR);

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

    m_activeGamepad = 0;
    m_imuEnabled = false;
    m_vibroEnabled = false;

    float accelScale = SDL_STANDARD_GRAVITY / SWITCH_ACCEL_SCALE;
    float gyroScale = M_PI / 180.0 / SWITCH_GYRO_SCALE;

    m_fAccelScaleX = accelScale;
    m_fAccelScaleY = accelScale;
    m_fAccelScaleZ = accelScale;

    m_fGyroScaleX = gyroScale;
    m_fGyroScaleY = gyroScale;
    m_fGyroScaleZ = gyroScale;
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
        SDL_WaitThread(m_thread, nullptr);
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

void JoyHandler::runner()
{
    m_running = true;

    SDL_Event evt;

    SDL_JoystickEventState(SDL_ENABLE);

    // Open already plugged joysticks
    int numJoysticks = SDL_NumJoysticks();

    for(int i = 0; i < numJoysticks; ++i)
        addJoystick(i);

    joysticksSyncList();

    // Start the watching
    while(m_running)
    {
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
        }

        QThread::msleep(1000 / 130);
    }

    for(ControllerEntry &it : m_joysticks)
        ctrlCleanUp(it);

    m_joysticks.clear();

    joysticksSyncList();
}

int JoyHandler::runner_s(void *data)
{
    JoyHandler *self = reinterpret_cast<JoyHandler*>(data);
    self->runner();
    return 0;
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



struct StickCalibration
{
    float centre_x;
    float centre_y;
    float min_x;
    float max_x;
    float min_y;
    float max_y;
};

// static const StickCalibration s_calib_left = {2159.0f, 1916.0f, -1466.0f, 1517.0f, -1583.0f, 1465.0f};
// static const StickCalibration s_calib_right = {2070.0f, 2013.0f, -1522.0f, 1414.0f, -1531.0f, 1510.0f};
static const StickCalibration s_calib_left = {2047.0f, 2047.0f, -2047.0f, 2047.0f, -2047.0f, 2047.0f};
static const StickCalibration s_calib_right = {2047.0f, 2047.0f, -2047.0f, 2047.0f, -2047.0f, 2047.0f};

static void axis2bin(uint16_t out[2], float ratio_x, float ratio_y, const StickCalibration &calib)
{
    float ret;

    if(ratio_x < 0)
        ret = SDL_fabsf(ratio_x) * calib.min_x + calib.centre_x;
    else
        ret = SDL_fabsf(ratio_x) * calib.max_x + calib.centre_x;

    out[0] = (int)SDL_round(ret);

    if(ratio_y < 0)
        ret = SDL_fabsf(ratio_y) * calib.min_y + calib.centre_y;
    else
        ret = SDL_fabsf(ratio_y) * calib.max_y + calib.centre_y;

    out[1] = (int)SDL_round(ret);
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


static Uint16 const c_rumbler_hfa[101][2] =
{
    { 0, 0x0 }, { 514, 0x2 }, { 775, 0x4 }, { 921, 0x6 }, { 1096, 0x8 },
    { 1303, 0x0a }, { 1550, 0x0c }, { 1843, 0x0e }, { 2192, 0x10 }, { 2606, 0x12 },
    { 3100, 0x14 }, { 3686, 0x16 }, { 4383, 0x18 }, { 5213, 0x1a }, { 6199, 0x1c },
    { 7372, 0x1e }, { 7698, 0x20 }, { 8039, 0x22 }, { 8395, 0x24 }, { 8767, 0x26 },
    { 9155, 0x28 }, { 9560, 0x2a }, { 9984, 0x2c }, { 10426, 0x2e }, { 10887, 0x30 },
    { 11369, 0x32 }, { 11873, 0x34 }, { 12398, 0x36 }, { 12947, 0x38 }, { 13520, 0x3a },
    { 14119, 0x3c }, { 14744, 0x3e }, { 15067, 0x40 }, { 15397, 0x42 }, { 15734, 0x44 },
    { 16079, 0x46 }, { 16431, 0x48 }, { 16790, 0x4a }, { 17158, 0x4c }, { 17534, 0x4e },
    { 17918, 0x50 }, { 18310, 0x52 }, { 18711, 0x54 }, { 19121, 0x56 }, { 19540, 0x58 },
    { 19967, 0x5a }, { 20405, 0x5c }, { 20851, 0x5e }, { 21308, 0x60 }, { 21775, 0x62 },
    { 22251, 0x64 }, { 22739, 0x66 }, { 23236, 0x68 }, { 23745, 0x6a }, { 24265, 0x6c },
    { 24797, 0x6e }, { 25340, 0x70 }, { 25894, 0x72 }, { 26462, 0x74 }, { 27041, 0x76 },
    { 27633, 0x78 }, { 28238, 0x7a }, { 28856, 0x7c }, { 29488, 0x7e }, { 30134, 0x80 },
    { 30794, 0x82 }, { 31468, 0x84 }, { 32157, 0x86 }, { 32861, 0x88 }, { 33581, 0x8a },
    { 34316, 0x8c }, { 35068, 0x8e }, { 35836, 0x90 }, { 36620, 0x92 }, { 37422, 0x94 },
    { 38242, 0x96 }, { 39079, 0x98 }, { 39935, 0x9a }, { 40809, 0x9c }, { 41703, 0x9e },
    { 42616, 0xa0 }, { 43549, 0xa2 }, { 44503, 0xa4 }, { 45477, 0xa6 }, { 46473, 0xa8 },
    { 47491, 0xaa }, { 48531, 0xac }, { 49593, 0xae }, { 50679, 0xb0 }, { 51789, 0xb2 },
    { 52923, 0xb4 }, { 54082, 0xb6 }, { 55266, 0xb8 }, { 56476, 0xba }, { 57713, 0xbc },
    { 58977, 0xbe }, { 60268, 0xc0 }, { 61588, 0xc2 }, { 62936, 0xc4 }, { 64315, 0xc6 },
    { 65535, 0xc8 }
};

static Uint16 c_rumbler_lfa[101][2] =
{
    { 0, 0x0040 }, { 514, 0x8040 }, { 775, 0x0041 }, { 921, 0x8041 }, { 1096, 0x0042 },
    { 1303, 0x8042 }, { 1550, 0x0043 }, { 1843, 0x8043 }, { 2192, 0x0044 }, { 2606, 0x8044 },
    { 3100, 0x0045 }, { 3686, 0x8045 }, { 4383, 0x0046 }, { 5213, 0x8046 }, { 6199, 0x0047 },
    { 7372, 0x8047 }, { 7698, 0x0048 }, { 8039, 0x8048 }, { 8395, 0x0049 }, { 8767, 0x8049 },
    { 9155, 0x004a }, { 9560, 0x804a }, { 9984, 0x004b }, { 10426, 0x804b }, { 10887, 0x004c },
    { 11369, 0x804c }, { 11873, 0x004d }, { 12398, 0x804d }, { 12947, 0x004e }, { 13520, 0x804e },
    { 14119, 0x004f }, { 14744, 0x804f }, { 15067, 0x0050 }, { 15397, 0x8050 }, { 15734, 0x0051 },
    { 16079, 0x8051 }, { 16431, 0x0052 }, { 16790, 0x8052 }, { 17158, 0x0053 }, { 17534, 0x8053 },
    { 17918, 0x0054 }, { 18310, 0x8054 }, { 18711, 0x0055 }, { 19121, 0x8055 }, { 19540, 0x0056 },
    { 19967, 0x8056 }, { 20405, 0x0057 }, { 20851, 0x8057 }, { 21308, 0x0058 }, { 21775, 0x8058 },
    { 22251, 0x0059 }, { 22739, 0x8059 }, { 23236, 0x005a }, { 23745, 0x805a }, { 24265, 0x005b },
    { 24797, 0x805b }, { 25340, 0x005c }, { 25894, 0x805c }, { 26462, 0x005d }, { 27041, 0x805d },
    { 27633, 0x005e }, { 28238, 0x805e }, { 28856, 0x005f }, { 29488, 0x805f }, { 30134, 0x0060 },
    { 30794, 0x8060 }, { 31468, 0x0061 }, { 32157, 0x8061 }, { 32861, 0x0062 }, { 33581, 0x8062 },
    { 34316, 0x0063 }, { 35068, 0x8063 }, { 35836, 0x0064 }, { 36620, 0x8064 }, { 37422, 0x0065 },
    { 38242, 0x8065 }, { 39079, 0x0066 }, { 39935, 0x8066 }, { 40809, 0x0067 }, { 41703, 0x8067 },
    { 42616, 0x0068 }, { 43549, 0x8068 }, { 44503, 0x0069 }, { 45477, 0x8069 }, { 46473, 0x006a },
    { 47491, 0x806a }, { 48531, 0x006b }, { 49593, 0x806b }, { 50679, 0x006c }, { 51789, 0x806c },
    { 52923, 0x006d }, { 54082, 0x806d }, { 55266, 0x006e }, { 56476, 0x806e }, { 57713, 0x006f },
    { 58977, 0x806f }, { 60268, 0x0070 }, { 61588, 0x8070 }, { 62936, 0x0071 }, { 64315, 0x8071 },
    { 65535, 0x0072 }
};

static const uint8_t c_rumbleNeutralHalf[4] = {0x00, 0x01, 0x40, 0x40};

struct NXRumbleMidVals
{
    uint16_t usHighFreq;
    uint16_t usLowAmp;
    uint8_t usHighAmp;
    uint8_t usLowFreq;
};

static Uint16 decodeRumbleHigh(Uint8 amp)
{
    int index = 0;

    for( ; index < 101; index++)
    {
        if(amp == c_rumbler_hfa[index][1])
            return c_rumbler_hfa[index][0];
    }

    return c_rumbler_hfa[100][0];
}

static Uint16 decodeRumbleLow(Uint16 amp)
{
    int index = 0;

    for( ; index < 101; index++)
    {
        if(amp == c_rumbler_lfa[index][1])
            return c_rumbler_lfa[index][0];
    }

    return c_rumbler_lfa[100][0];
}


static void decodeRumble(const QByteArray &rumbleData, size_t half, Uint16 *low_freq_rumble, Uint16 *high_freq_rumble)
{
    if(rumbleData.size() < 8 || half > 1)
        return; // Invalid!

    NXRumbleMidVals vals;
    const uint8_t *rumble = (const uint8_t *)rumbleData.data() + (half * 4);

    if(SDL_memcmp(rumble, c_rumbleNeutralHalf, 4) == 0)
    {
        SDL_memset(&vals, 0, sizeof(vals));
        *low_freq_rumble = 0;
        *high_freq_rumble = 0;
    }
    else
    {
        vals.usHighFreq = (uint16_t)rumble[0] | ((uint16_t)(rumble[1] & 0x01) << 8);
        vals.usHighAmp = rumble[1] & 0xFE;

        vals.usLowFreq = rumble[2] & 0x7F;
        vals.usLowAmp = (uint16_t)rumble[3] | ((uint16_t)(rumble[2] & 0x80) << 8);

        *low_freq_rumble = decodeRumbleLow(vals.usLowAmp);
        *high_freq_rumble = decodeRumbleHigh(vals.usHighAmp);
    }
}

bool JoyHandler::joystickProcess(ControllerEntry &entry, bool *buttons, bool *tilting)
{
    uint16_t tmp[2], rumble_l = 0, rumble_h = 0;
    NXSendCmd bOld, tOld;
    memcpy(&bOld, &m_buttons, sizeof(NXSendCmd));
    memcpy(&tOld, &m_tilting, sizeof(NXSendCmd));

    *buttons = false;
    *tilting = false;

    // Read buttons state

    float axis_l_x_val = processAxis(entry.ctrl, SDL_CONTROLLER_AXIS_LEFTX);
    float axis_l_y_val = -processAxis(entry.ctrl, SDL_CONTROLLER_AXIS_LEFTY);
    float axis_r_x_val = processAxis(entry.ctrl, SDL_CONTROLLER_AXIS_RIGHTX);
    float axis_r_y_val = -processAxis(entry.ctrl, SDL_CONTROLLER_AXIS_RIGHTY);

    axis_l_x_val = filterDead(axis_l_x_val, axis_l_x_val, axis_l_y_val);
    axis_l_y_val = filterDead(axis_l_y_val, axis_l_x_val, axis_l_y_val);
    axis_r_x_val = filterDead(axis_r_x_val, axis_r_x_val, axis_r_y_val);
    axis_r_y_val = filterDead(axis_r_y_val, axis_r_x_val, axis_r_y_val);

    axis2bin(tmp, axis_l_x_val, axis_l_y_val, s_calib_left);
    m_buttons.data.state.stick_l[0] = tmp[0];
    m_buttons.data.state.stick_l[1] = tmp[1];

    axis2bin(tmp, axis_r_x_val, axis_r_y_val, s_calib_right);
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

    if(m_imuEnabled)
    {
        // Read tilting state if available
        if(entry.has_gyro)
        {
            float gyro[3];
            SDL_GameControllerGetSensorData(entry.ctrl, SDL_SENSOR_GYRO, gyro, 3);

            m_tilting.data.tilt.gyro[0] = -(int16_t)(gyro[2] / m_fGyroScaleX);
            m_tilting.data.tilt.gyro[1] = -(int16_t)(gyro[0] / m_fGyroScaleY);
            m_tilting.data.tilt.gyro[2] = (int16_t)(gyro[1] / m_fGyroScaleZ);
        }

        if(entry.has_accel)
        {
            float accel[3];
            SDL_GameControllerGetSensorData(entry.ctrl, SDL_SENSOR_ACCEL, accel, 3);

            m_tilting.data.tilt.accel[0] = -(int16_t)(accel[2] / m_fAccelScaleX);
            m_tilting.data.tilt.accel[1] = -(int16_t)(accel[0] / m_fAccelScaleY);
            m_tilting.data.tilt.accel[2] = (int16_t)(accel[1] / m_fAccelScaleZ);
        }

        if(memcmp(&m_tilting.data.tilt, &tOld.data.tilt, sizeof(m_tilting.data.tilt)) != 0)
            *tilting = true;
    }

    // Run rumble
    m_rumbleMutex.lock();
    if(!m_rumble.isEmpty())
    {
        auto data = m_rumble;
        m_rumble.clear();

        decodeRumble(data, 0, &rumble_l, &rumble_h);

        if(rumble_l || rumble_h)
            qDebug() << "Rumble:" << rumble_l << rumble_h;

        SDL_GameControllerRumble(entry.ctrl, rumble_l, rumble_h, 17);
    }
    m_rumbleMutex.unlock();

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
