#ifndef JOY_HANDLER_H
#define JOY_HANDLER_H

#include <QObject>
#include <QMutex>
#include <QMap>
#include <QAtomicInteger>
#include <thread>

#include "buttons.h"

typedef int32_t SDL_JoystickID;

struct _SDL_Joystick;
typedef struct _SDL_Joystick SDL_Joystick;

struct _SDL_GameController;
typedef struct _SDL_GameController SDL_GameController;

class JoyHandler : public QObject
{
    Q_OBJECT

public:
    explicit JoyHandler(QObject *o = nullptr);
    virtual ~JoyHandler();

    void start();
    void stop();

    void setIMU(bool en);
    void playRunbleData(const QByteArray &data);

    QStringList getJoysticks();
    int numJoysticks();

    void selectActiveGamepad(int index);

signals:
    void debugMessage(const QString &msg);
    void sendCommand(const NXSendCmd &cmd);
    void joysticksListUpdated();

private:
    void runner();
    QAtomicInteger<bool> m_running;
    QAtomicInteger<bool> m_imuEnabled;
    QAtomicInteger<int> m_activeGamepad;
    std::thread m_thread;
    QMutex m_mutex;

    QByteArray m_rumble;

    NXSendCmd m_buttons;
    NXSendCmd m_tilting;

    float m_fAccelScaleX;
    float m_fAccelScaleY;
    float m_fAccelScaleZ;

    float m_fGyroScaleX;
    float m_fGyroScaleY;
    float m_fGyroScaleZ;

    struct ControllerEntry
    {
        QString name;
        SDL_Joystick *joy = nullptr;
        SDL_GameController *ctrl = nullptr;
        bool swap_buttons = false;
        bool has_gyro = false;
        bool has_accel = false;
    };

    static void ctrlCleanUp(ControllerEntry &entry);

    void addJoystick(int joyIdx);
    void removeJoystick(int instance);
    bool joystickProcess(ControllerEntry &entry, bool *buttons, bool *tilting);


    QMap<SDL_JoystickID, ControllerEntry> m_joysticks;
};

#endif // JOY_HANDLER_H
