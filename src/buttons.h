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

// To loop through all button states:
// for(uint32_t i = 0; i < BUTTON_END; i <<= 1)

enum NXCommand
{
    CMD_None = 0,
    CMD_ButtonsUpdate = 1,  // 14
    CMD_Tilt = 2,           // 14
    CMD_Ping = 3,           // 1
    CMD_Reset = 4,          // 1
    CMD_QueryPlayer = 5,    // 1
    CMD_QueryIMU = 6,       // 1
    CMD_QueryVibro = 7,     // 1
    CMD_QueryConfig = 8,    // 1

    CMD_END
};

enum NXPlayer
{
    LED_PLAYER_1 = 0x01,
    LED_PLAYER_2 = 0x02,
    LED_PLAYER_3 = 0x04,
    LED_PLAYER_4 = 0x08,
    LED_PLAYER_1_FLASH = 0x10,
    LED_PLAYER_2_FLASH = 0x20,
    LED_PLAYER_3_FLASH = 0x40,
    LED_PLAYER_4_FLASH = 0x80,
};

struct __attribute__((packed)) NXSendCmd
{
    // Command of @NXCommand type
    uint8_t cmd;

    union Data
    {
        struct Buttons
        {
            uint32_t buttons;
            uint16_t stick_l[2];
            uint16_t stick_r[2];
        } state;

        // Accelerometer / Gyroscope X, Y, Z
        struct Tilt
        {
            int16_t accel[3];
            int16_t gyro[3];
        } tilt;

        struct Tail
        {
            uint8_t tail[12];
        } tail;
    } data;

    uint8_t tail;
};


#endif // BUTTONS_H
