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

void JoyHandler::playRunbleData(const QByteArray &data)
{
    m_mutex.lock();
    m_rumble.append(data);
    m_mutex.unlock();
}

QStringList JoyHandler::getJoysticks()
{
    QStringList ret;

    m_mutex.lock();

    for(ControllerEntry &it : m_joysticks)
        ret.push_back(it.name);

    m_mutex.unlock();

    return ret;
}

int JoyHandler::numJoysticks()
{
    int ret;

    m_mutex.lock();
    ret = m_joysticks.size();
    m_mutex.unlock();

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

    emit joysticksListUpdated();

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
                emit joysticksListUpdated();
                break;

            case SDL_JOYDEVICEREMOVED:
                emit debugMessage(QString("-- Disconnecting old gamepad! Instance %1").arg(evt.jdevice.which));
                removeJoystick(evt.jdevice.which);
                emit joysticksListUpdated();
                break;
            }
        }

        m_mutex.lock();

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

        m_mutex.unlock();

        QThread::msleep(1000 / 130);
    }

    m_mutex.lock();
    for(ControllerEntry &it : m_joysticks)
        ctrlCleanUp(it);
    m_mutex.unlock();
    m_joysticks.clear();
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
    }

    SDL_JoystickID instance = SDL_JoystickInstanceID(entry.joy);

    m_mutex.lock();
    m_joysticks.insert(instance, std::move(entry));
    m_mutex.unlock();

}

void JoyHandler::removeJoystick(int instance)
{
    m_mutex.lock();

    auto it = m_joysticks.find(instance);

    if(it != m_joysticks.end())
    {
        ControllerEntry entry = it.value();
        ctrlCleanUp(entry);
        m_joysticks.remove(instance);
    }

    m_mutex.unlock();
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


bool JoyHandler::joystickProcess(ControllerEntry &entry, bool *buttons, bool *tilting)
{
    uint16_t tmp[2];
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

    return *tilting || *buttons;
}

