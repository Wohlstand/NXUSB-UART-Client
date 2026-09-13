#ifndef BUTTONS_H
#define BUTTONS_H

#include <stdint.h>

enum NxBtButtons
{
    BUTTON_Y               = 0x00010000,
    BUTTON_X               = 0x00020000,
    BUTTON_B               = 0x00040000,
    BUTTON_A               = 0x00080000,
    BUTTON_JCR_SR          = 0x00100000,
    BUTTON_JCR_SL          = 0x00200000,
    BUTTON_R               = 0x00400000,
    BUTTON_ZR              = 0x00800000,

    BUTTON_PLUS            = 0x00000100,
    BUTTON_MINUS           = 0x00000200,
    BUTTON_R_STICK_PRESS   = 0x00000400,
    BUTTON_L_STICK_PRESS   = 0x00000800,
    BUTTON_HOME            = 0x00001000,
    BUTTON_CAPTURE         = 0x00002000,
    BUTTON_PADDING         = 0x00004000,
    BUTTON_CHARGING        = 0x00008000,

    BUTTON_DPAD_DOWN       = 0x00000001,
    BUTTON_DPAD_UP         = 0x00000002,
    BUTTON_DPAD_RIGHT      = 0x00000004,
    BUTTON_DPAD_LEFT       = 0x00000008,
    BUTTON_JCL_SR          = 0x00000010,
    BUTTON_JCL_SL          = 0x00000020,
    BUTTON_L               = 0x00000040,
    BUTTON_ZL              = 0x00000080,

    BUTTON_BEGIN = BUTTON_DPAD_DOWN,
    BUTTON_END = (BUTTON_ZR << 1)
};

enum NXCommand
{
    CMD_None = 0,
    CMD_ButtonsUpdate = 1,
    CMD_Tilt = 2,
    CMD_Ping = 3,
    CMD_Reset = 4,
    CMD_QueryPlayer = 5,
    CMD_QueryIMU = 6,
};

struct __attribute__((packed)) NXSendCmd
{
    // Command of @NXCommand type
    uint8_t cmd;

    union
    {
        struct Buttons
        {
            uint32_t buttons;
            uint16_t stick_l[2];
            uint16_t stick_r[2];
        } state;

        // Accelerometer / Gyroscope X, Y, Z, 3 samples
        struct Tilt
        {
            int16_t accel[3];
            int16_t gyro[3];
        } tilt;

        struct Padding
        {
            uint8_t junk[12];
        } pad;
    } data;

    uint8_t tail;
};


#endif // BUTTONS_H
