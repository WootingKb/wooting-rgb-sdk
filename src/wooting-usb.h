/*
 * Copyright 2018 Wooting Technologies B.V.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
#ifdef WOOTINGRGBSDK_EXPORTS
#define WOOTINGRGBSDK_API __declspec(dllexport)
#else
#define WOOTINGRGBSDK_API __declspec(dllimport)
#endif
#else
// __declspec is win32 only
#define WOOTINGRGBSDK_API
#endif

#include "stdbool.h"
#include "stdint.h"
#include <stddef.h>
#include <stdio.h>

typedef void (*void_cb)(void);

typedef enum RGB_PARTS { PART0, PART1, PART2, PART3, PART4 } RGB_PARTS;

typedef enum WOOTING_DEVICE_TYPE {
  // 10 Keyless Keyboard. E.g. Wooting One
  DEVICE_KEYBOARD_TKL = 1,

  // Full Size keyboard. E.g. Wooting Two
  DEVICE_KEYBOARD = 2,

  // 60 percent keyboard, E.g. Wooting 60HE
  DEVICE_KEYBOARD_60 = 3
} WOOTING_DEVICE_TYPE;

typedef enum WOOTING_DEVICE_LAYOUT {
  LAYOUT_UNKNOWN = -1,
  LAYOUT_ANSI = 0,
  LAYOUT_ISO = 1
} WOOTING_DEVICE_LAYOUT;

typedef struct WOOTING_USB_META {
  bool connected;
  const char *model;
  uint8_t max_rows;
  uint8_t max_columns;
  uint8_t led_index_max;
  WOOTING_DEVICE_TYPE device_type;
  bool v2_interface;
  WOOTING_DEVICE_LAYOUT layout;
} WOOTING_USB_META;

typedef struct _KeyboardMatrixID {
  uint8_t column : 5;
  uint8_t row : 3;
} KeyboardMatrixID;

#define WOOTING_MAX_RGB_DEVICES 10

#define RGB_RAW_BUFFER_SIZE 96

#define WOOTING_RGB_ROWS 6
#define WOOTING_RGB_COLS 21
#define WOOTING_ONE_RGB_COLS 17
#define WOOTING_TWO_RGB_COLS 21

#define WOOTING_ONE_KEY_CODE_LIMIT 95
#define WOOTING_TWO_KEY_CODE_LIMIT 116
#define WOOTING_KEY_CODE_LIMIT WOOTING_TWO_KEY_CODE_LIMIT

#define WOOTING_RAW_COLORS_REPORT 11
#define WOOTING_DEVICE_CONFIG_COMMAND 19
#define WOOTING_SINGLE_COLOR_COMMAND 30
#define WOOTING_SINGLE_RESET_COMMAND 31
#define WOOTING_RESET_ALL_COMMAND 32
#define WOOTING_COLOR_INIT_COMMAND 33

void wooting_usb_set_disconnected_cb(void_cb cb);
void wooting_usb_disconnect(bool trigger_cb);

bool wooting_usb_find_keyboard(void);

WOOTING_USB_META *wooting_usb_get_meta(void);

/// @brief Gets the meta struct of a particular device
/// @param device_index Index of the device you want the meta of
/// @return Pointer to the meta struct of the device, NULL if out of range
WOOTINGRGBSDK_API WOOTING_USB_META *
wooting_usb_get_device_meta(uint8_t device_index);

/// @brief Returns the number of devices connected
/// @return The number of devices connected
WOOTINGRGBSDK_API uint8_t wooting_usb_device_count(void);

/// @brief Selects a particular device as the receiver of following commands
/// @param  device_index The index of the device to select
/// @return true if the device was selected, false if the device index was out
/// of range
WOOTINGRGBSDK_API bool wooting_usb_select_device(uint8_t);

WOOTINGRGBSDK_API bool wooting_usb_use_v2_interface(void);
WOOTINGRGBSDK_API size_t wooting_usb_get_response_size(void);

WOOTINGRGBSDK_API bool wooting_usb_send_buffer_v1(RGB_PARTS part_number,
                                                  uint8_t rgb_buffer[]);
WOOTINGRGBSDK_API bool wooting_usb_send_buffer_v2(
    uint16_t rgb_buffer[WOOTING_RGB_ROWS][WOOTING_RGB_COLS]);
WOOTINGRGBSDK_API bool wooting_usb_send_feature(uint8_t commandId,
                                                uint8_t parameter0,
                                                uint8_t parameter1,
                                                uint8_t parameter2,
                                                uint8_t parameter3);
WOOTINGRGBSDK_API int wooting_usb_send_feature_with_response(
    uint8_t *buff, size_t len, uint8_t commandId, uint8_t parameter0,
    uint8_t parameter1, uint8_t parameter2, uint8_t parameter3);

WOOTINGRGBSDK_API int
wooting_usb_read_response_timeout(uint8_t *buff, size_t len, int milliseconds);
WOOTINGRGBSDK_API int wooting_usb_read_response(uint8_t *buff, size_t len);

#ifdef __cplusplus
}
#endif
