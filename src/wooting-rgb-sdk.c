/*
 * Copyright 2018 Wooting Technologies B.V.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
#include "wooting-rgb-sdk.h"

/** @brief Builds the V1 buffers from a full matrix

This function makes the SDK backwards compatible by building the V1 split
buffers from the full RGB matrix of V2 devices.

@returns
Will return true(1) after building the buffers
*/
bool wooting_rgb_build_v1_buffers();

#define NOLED 255
#define LED_LEFT_SHIFT_ANSI 9
#define LED_LEFT_SHIFT_ISO 7
#define LED_ENTER_ANSI 65
#define LED_ENTER_ISO 62

static bool wooting_rgb_auto_update = false;

// Each rgb buffer is able to hold RGB values for 24 keys
// There is some overhead because of the memory layout of the LED drivers
static uint8_t rgb_buffer0[RGB_RAW_BUFFER_SIZE] = {0};
static uint8_t rgb_buffer1[RGB_RAW_BUFFER_SIZE] = {0};
static uint8_t rgb_buffer2[RGB_RAW_BUFFER_SIZE] = {0};
static uint8_t rgb_buffer3[RGB_RAW_BUFFER_SIZE] = {0};
static uint8_t rgb_buffer4[RGB_RAW_BUFFER_SIZE] = {0};

static uint8_t gammaFilter[256] = {
    0,   0,   2,   2,   2,   2,   2,   2,   2,   2,   2,   2,   2,   2,   2,
    2,   2,   2,   3,   3,   3,   3,   3,   3,   3,   4,   4,   4,   4,   4,
    5,   5,   5,   5,   6,   6,   6,   6,   7,   7,   7,   7,   8,   8,   8,
    9,   9,   9,   10,  10,  10,  10,  10,  10,  11,  11,  11,  11,  11,  11,
    12,  12,  13,  13,  13,  13,  14,  14,  15,  15,  16,  16,  17,  17,  18,
    18,  18,  18,  19,  19,  20,  20,  21,  21,  22,  22,  22,  22,  23,  24,
    24,  25,  25,  26,  26,  26,  27,  27,  28,  29,  29,  30,  31,  32,  32,
    33,  34,  35,  35,  36,  37,  38,  39,  39,  40,  41,  42,  42,  43,  43,
    44,  44,  45,  45,  46,  47,  48,  49,  50,  50,  51,  51,  52,  52,  53,
    54,  54,  55,  55,  56,  56,  57,  57,  58,  58,  59,  59,  60,  60,  61,
    61,  62,  62,  63,  64,  65,  66,  67,  68,  69,  70,  71,  72,  73,  74,
    75,  77,  78,  79,  81,  82,  83,  85,  86,  87,  89,  90,  92,  93,  95,
    96,  98,  99,  101, 102, 104, 105, 107, 109, 110, 112, 114, 115, 117, 119,
    120, 122, 124, 126, 127, 129, 131, 133, 135, 137, 138, 140, 142, 144, 146,
    148, 150, 152, 154, 156, 158, 160, 162, 164, 167, 169, 171, 173, 175, 177,
    180, 182, 184, 186, 189, 191, 193, 196, 198, 200, 203, 205, 208, 210, 213,
    215, 218, 220, 223, 225, 228, 231, 233, 236, 239, 241, 244, 247, 249, 252,
    255};

static WOOTING_RGB_MATRIX rgb_buffer_matrix_array[WOOTING_MAX_RGB_DEVICES];
static WOOTING_RGB_MATRIX *rgb_buffer_matrix;

// Converts the array index to a memory location in the RGB buffers
static uint8_t get_safe_led_idex(uint8_t row, uint8_t column) {
  const uint8_t rgb_led_index[WOOTING_RGB_ROWS][WOOTING_RGB_COLS] = {
      {0,  NOLED, 11, 12, 23, 24, 36,  47,  85,  84, 49,
       48, 59,    61, 73, 81, 80, 113, 114, 115, 116},
      {2,  1,  14, 13, 26, 25, 35, 38, 37, 87, 86,
       95, 51, 63, 75, 72, 74, 96, 97, 98, 99},
      {3,  4,  15, 16, 27, 28, 39,  42,  40,  88, 89,
       52, 53, 71, 76, 83, 77, 102, 103, 104, 100},
      {5,  6,  17, 18,    29,    30,    41,  46,  44,  90,   93,
       54, 57, 65, NOLED, NOLED, NOLED, 105, 106, 107, NOLED},
      {9,  8,     19, 20,    31, 34,    32,  45,  43,  91, 92,
       55, NOLED, 66, NOLED, 78, NOLED, 108, 109, 110, 101},
      {10, 22, 21, NOLED, NOLED, NOLED, 33,    NOLED, NOLED, NOLED, 94,
       58, 67, 68, 70,    79,    82,    NOLED, 111,   112,   NOLED}};

  WOOTING_USB_META *meta = wooting_usb_get_meta();
  if (row < meta->max_rows && column < meta->max_columns) {
    return rgb_led_index[row][column];
  } else {
    return NOLED;
  }
}

static inline uint16_t encodeColor(uint8_t red, uint8_t green, uint8_t blue) {
  uint16_t encode = 0;

  encode |= (red & 0xf8) << 8;
  encode |= (green & 0xfc) << 3;
  encode |= (blue & 0xf8) >> 3;

  return encode;
}

static inline void decodeColor(uint16_t color, uint8_t *red, uint8_t *green,
                               uint8_t *blue) {
  *red = (color >> 8) & 0xf8;
  *green = (color >> 3) & 0xfc;
  *blue = (color << 3) & 0xf8;
}

bool wooting_rgb_kbd_connected() { return wooting_usb_find_keyboard(); }

void wooting_rgb_set_disconnected_cb(void_cb cb) {
  wooting_usb_set_disconnected_cb(cb);
}

bool wooting_rgb_reset_rgb() {
  return wooting_usb_send_feature(WOOTING_RESET_ALL_COMMAND, 0, 0, 0, 0);
}

bool wooting_rgb_close() {
  if (wooting_rgb_reset_rgb()) {
    wooting_usb_disconnect(false);
    return true;
  } else {
    return false;
  }
}

bool wooting_rgb_reset() { return wooting_rgb_close(); }

bool wooting_rgb_direct_set_key(uint8_t row, uint8_t column, uint8_t red,
                                uint8_t green, uint8_t blue) {
  // We don't need to call this here as the wooting_usb_send_feature calls will
  // perform the check again and there's no need to run this multiple times,
  // especially when each call will attempt to ensure connection is still
  // available if (!wooting_rgb_kbd_connected()) { 	return false;
  // }
  if (wooting_usb_use_v2_interface()) {
    KeyboardMatrixID id = {.row = row, .column = column};
    return wooting_usb_send_feature(WOOTING_SINGLE_COLOR_COMMAND,
                                    *(uint8_t *)&id, red, green, blue);
  } else {
    uint8_t keyCode = get_safe_led_idex(row, column);

    if (keyCode == NOLED || keyCode > wooting_usb_get_meta()->led_index_max) {
      return false;
    } else if (keyCode == LED_LEFT_SHIFT_ANSI) {
      bool update_ansi = wooting_usb_send_feature(
          WOOTING_SINGLE_COLOR_COMMAND, LED_LEFT_SHIFT_ANSI, red, green, blue);
      bool update_iso = wooting_usb_send_feature(
          WOOTING_SINGLE_COLOR_COMMAND, LED_LEFT_SHIFT_ISO, red, green, blue);

      return update_ansi && update_iso;
    } else if (keyCode == LED_ENTER_ANSI) {
      bool update_ansi = wooting_usb_send_feature(
          WOOTING_SINGLE_COLOR_COMMAND, LED_ENTER_ANSI, red, green, blue);
      bool update_iso = wooting_usb_send_feature(
          WOOTING_SINGLE_COLOR_COMMAND, LED_ENTER_ISO, red, green, blue);

      return update_ansi && update_iso;
    } else {
      return wooting_usb_send_feature(WOOTING_SINGLE_COLOR_COMMAND, keyCode,
                                      red, green, blue);
    }
  }
}

bool wooting_rgb_direct_reset_key(uint8_t row, uint8_t column) {
  if (!wooting_rgb_kbd_connected()) {
    return false;
  }

  if (wooting_usb_use_v2_interface()) {
    KeyboardMatrixID id = {.row = row, .column = column};
    return wooting_usb_send_feature(WOOTING_SINGLE_RESET_COMMAND, 0, 0, 0,
                                    *(uint8_t *)&id);
  } else {
    uint8_t keyCode = get_safe_led_idex(row, column);

    if (keyCode == NOLED || keyCode > wooting_usb_get_meta()->led_index_max) {
      return false;
    } else if (keyCode == LED_LEFT_SHIFT_ANSI) {
      bool update_ansi = wooting_usb_send_feature(WOOTING_SINGLE_RESET_COMMAND,
                                                  0, 0, 0, LED_LEFT_SHIFT_ANSI);
      bool update_iso = wooting_usb_send_feature(WOOTING_SINGLE_RESET_COMMAND,
                                                 0, 0, 0, LED_LEFT_SHIFT_ISO);

      return update_ansi && update_iso;
    } else if (keyCode == LED_ENTER_ANSI) {
      bool update_ansi = wooting_usb_send_feature(WOOTING_SINGLE_RESET_COMMAND,
                                                  0, 0, 0, LED_ENTER_ANSI);
      bool update_iso = wooting_usb_send_feature(WOOTING_SINGLE_RESET_COMMAND,
                                                 0, 0, 0, LED_ENTER_ISO);

      return update_ansi && update_iso;
    } else {
      return wooting_usb_send_feature(WOOTING_SINGLE_RESET_COMMAND, 0, 0, 0,
                                      keyCode);
    }
  }
}

void wooting_rgb_array_auto_update(bool auto_update) {
  wooting_rgb_auto_update = auto_update;
}

bool wooting_rgb_array_update_keyboard() {
  if (!wooting_rgb_kbd_connected()) {
    return false;
  }

  if (wooting_usb_use_v2_interface()) {
    if (!wooting_usb_send_buffer_v2(*rgb_buffer_matrix)) {
      return false;
    }
  } else {
    if (!wooting_rgb_build_v1_buffers())
      return false;

    if (!wooting_usb_send_buffer_v1(PART0, rgb_buffer0)) {
      return false;
    }

    if (!wooting_usb_send_buffer_v1(PART1, rgb_buffer1)) {
      return false;
    }

    if (!wooting_usb_send_buffer_v1(PART2, rgb_buffer2)) {
      return false;
    }

    if (!wooting_usb_send_buffer_v1(PART3, rgb_buffer3)) {
      return false;
    }

    if (wooting_usb_get_meta()->device_type == DEVICE_KEYBOARD) {
      if (!wooting_usb_send_buffer_v1(PART4, rgb_buffer4)) {
        return false;
      }
    }
  }
  return true;
}

static bool wooting_rgb_array_change_single(uint8_t row, uint8_t column,
                                            uint8_t red, uint8_t green,
                                            uint8_t blue) {
  uint16_t prevValue = (*rgb_buffer_matrix)[row][column];
  uint16_t newValue = encodeColor(red, green, blue);
  if (newValue != prevValue) {
    (*rgb_buffer_matrix)[row][column] = newValue;
  }

  return true;
}

bool wooting_rgb_array_set_single(uint8_t row, uint8_t column, uint8_t red,
                                  uint8_t green, uint8_t blue) {
  // We just check to see if we believe the keyboard to be connected here as
  // this call may just be updating the array and not actually communicating
  // with the keyboard If auto update is on then the update_keyboard method will
  // ping the keyboard before communicating with the keyboard
  if (!wooting_usb_get_meta()->connected) {
    return false;
  }

  if (!wooting_rgb_array_change_single(row, column, red, green, blue)) {
    return false;
  }

  if (wooting_rgb_auto_update) {
    return wooting_rgb_array_update_keyboard();
  } else {
    return true;
  }
}

bool wooting_rgb_array_set_full(const uint8_t *colors_buffer) {
  // Just need to check if we believe it is connected, the update_keyboard call
  // will ping the keyboard if it is necessary
  if (!wooting_usb_get_meta()->connected) {
    return false;
  }

  const uint8_t columns = wooting_usb_get_meta()->max_columns;

  for (uint8_t row = 0; row < WOOTING_RGB_ROWS; row++) {
    const uint8_t *color = colors_buffer + row * WOOTING_RGB_COLS * 3;
    for (uint8_t col = 0; col < columns; col++) {
      uint8_t red = color[0];
      uint8_t green = color[1];
      uint8_t blue = color[2];

      wooting_rgb_array_change_single(row, col, red, green, blue);

      color += 3;
    }
  }

  if (wooting_rgb_auto_update) {
    return wooting_rgb_array_update_keyboard();
  } else {
    return true;
  }
}

bool wooting_rgb_build_v1_buffers() {
  const uint8_t pwm_mem_map[48] = {
      0x0,  0x1,  0x2,  0x3,  0x4,  0x5,  0x8,  0x9,  0xa,  0xb,  0xc,  0xd,
      0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d,
      0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d,
      0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d};

  uint8_t *buffer_pointer;

  for (int row = 0; row < WOOTING_RGB_ROWS; row++) {
    for (int column = 0; column < WOOTING_RGB_COLS; column++) {
      uint8_t led_index = get_safe_led_idex(row, column);

      // prevent assigning led's that don't exist
      if (led_index > wooting_usb_get_meta()->led_index_max) {
        continue;
      }
      if (led_index >= 96) {
        buffer_pointer = rgb_buffer4;
      } else if (led_index >= 72) {
        buffer_pointer = rgb_buffer3;
      } else if (led_index >= 48) {
        buffer_pointer = rgb_buffer2;
      } else if (led_index >= 24) {
        buffer_pointer = rgb_buffer1;
      } else {
        buffer_pointer = rgb_buffer0;
      }

      uint8_t buffer_index = pwm_mem_map[led_index % 24];
      uint16_t key_colour = (*rgb_buffer_matrix)[row][column];

      uint8_t red, green, blue;
      decodeColor(key_colour, &red, &green, &blue);

      buffer_pointer[buffer_index] = gammaFilter[red];
      buffer_pointer[buffer_index + 0x10] = gammaFilter[green];
      buffer_pointer[buffer_index + 0x20] = gammaFilter[blue];

      if (led_index == LED_ENTER_ANSI) {
        uint8_t iso_enter_index = pwm_mem_map[LED_ENTER_ISO - 48];
        rgb_buffer2[iso_enter_index] = rgb_buffer2[buffer_index];
        rgb_buffer2[iso_enter_index + 0x10] = rgb_buffer2[buffer_index + 0x10];
        rgb_buffer2[iso_enter_index + 0x20] = rgb_buffer2[buffer_index + 0x20];
      } else if (led_index == LED_LEFT_SHIFT_ANSI) {
        uint8_t iso_shift_index = pwm_mem_map[LED_LEFT_SHIFT_ISO];
        rgb_buffer0[iso_shift_index] = rgb_buffer0[buffer_index];
        rgb_buffer0[iso_shift_index + 0x10] = rgb_buffer0[buffer_index + 0x10];
        rgb_buffer0[iso_shift_index + 0x20] = rgb_buffer0[buffer_index + 0x20];
      }
    }
  }

  return true;
}

bool wooting_rgb_select_buffer(uint8_t buffer_index) {
  // Fetch pointer and buffer data from arrays
  rgb_buffer_matrix = &rgb_buffer_matrix_array[buffer_index];

  return true;
}

const WOOTING_USB_META *wooting_rgb_device_info() {
  if (!wooting_usb_get_meta()->connected)
    wooting_usb_find_keyboard();
  return wooting_usb_get_meta();
}

WOOTING_DEVICE_LAYOUT wooting_rgb_device_layout(void) {
  return wooting_usb_get_meta()->layout;
}