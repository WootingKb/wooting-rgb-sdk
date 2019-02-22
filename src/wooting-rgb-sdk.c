/*
* Copyright 2018 Wooting Technologies B.V.
*
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
#include "stdbool.h"
#include "stdint.h"
#include "wooting-usb.h"
#include "wooting-rgb-sdk.h"

#define NOLED 255
#define LED_LEFT_SHIFT_ANSI 9
#define LED_LEFT_SHIFT_ISO 7
#define LED_ENTER_ANSI 65
#define LED_ENTER_ISO 62

static bool wooting_rgb_auto_update = false;

// Each rgb buffer is able to hold RGB values for 24 keys
// There is some overhead because of the memory layout of the LED drivers
static uint8_t rgb_buffer0[RGB_RAW_BUFFER_SIZE] = { 0 };
static uint8_t rgb_buffer1[RGB_RAW_BUFFER_SIZE] = { 0 };
static uint8_t rgb_buffer2[RGB_RAW_BUFFER_SIZE] = { 0 };
static uint8_t rgb_buffer3[RGB_RAW_BUFFER_SIZE] = { 0 };
static uint8_t rgb_buffer4[RGB_RAW_BUFFER_SIZE] = { 0 };

static bool rgb_buffer0_changed = false;
static bool rgb_buffer1_changed = false;
static bool rgb_buffer2_changed = false;
static bool rgb_buffer3_changed = false;
static bool rgb_buffer4_changed = false;

// Converts the array index to a memory location in the RGB buffers
static uint8_t get_safe_led_idex(uint8_t row, uint8_t column) {
	const uint8_t rgb_led_index[WOOTING_RGB_ROWS][WOOTING_RGB_COLS] = {
		{ 0, NOLED, 11, 12, 23, 24, 36, 47, 85, 84, 49, 48, 59, 61, 73, 81, 80, 113, 114, 115, 116 },
		{ 2, 1, 14, 13, 26, 25, 35, 38, 37, 87, 86, 95, 51, 63, 75, 72, 74, 96, 97, 98, 99 },
		{ 3, 4, 15, 16, 27, 28, 39, 42, 40, 88, 89, 52, 53, 71, 76, 83, 77, 102, 103, 104, 100 },
		{ 5, 6, 17, 18, 29, 30, 41, 46, 44, 90, 93, 54, 57, 65, NOLED, NOLED, NOLED, 105, 106, 107, NOLED },
		{ 9, 8, 19, 20, 31, 34, 32, 45, 43, 91, 92, 55, NOLED, 66, NOLED, 78, NOLED, 108, 109, 110, 101 },
		{ 10, 22, 21, NOLED, NOLED, NOLED, 33, NOLED, NOLED, NOLED, 94, 58, 67, 68, 70, 79, 82, NOLED, 111, 112, NOLED }
	};

	if (row < WOOTING_RGB_ROWS && column < WOOTING_RGB_COLS) {
		return rgb_led_index[row][column];
	} else {
		return NOLED;
	}
}

bool wooting_rgb_kbd_connected() {
	return wooting_usb_find_keyboard();
}

void wooting_rgb_set_disconnected_cb(void_cb cb) {
	wooting_usb_set_disconnected_cb(cb);
}

bool wooting_rgb_reset() {
	if (wooting_usb_send_feature(WOOTING_RESET_ALL_COMMAND, 0, 0, 0, 0)) {
		wooting_usb_disconnect(false);
	}
	else {
		return false;
	}
}

bool wooting_rgb_direct_set_key(uint8_t row, uint8_t column, uint8_t red, uint8_t green, uint8_t blue) {
	uint8_t keyCode = get_safe_led_idex(row, column);

	if (keyCode == NOLED) {
		return false;
	}
	else if (keyCode == LED_LEFT_SHIFT_ANSI) {
		bool update_ansi = wooting_usb_send_feature(WOOTING_SINGLE_COLOR_COMMAND, LED_LEFT_SHIFT_ANSI, red, green, blue);
		bool update_iso = wooting_usb_send_feature(WOOTING_SINGLE_COLOR_COMMAND, LED_LEFT_SHIFT_ISO, red, green, blue);

		return update_ansi && update_iso;
	}
	else if (keyCode == LED_ENTER_ANSI) {
		bool update_ansi = wooting_usb_send_feature(WOOTING_SINGLE_COLOR_COMMAND, LED_ENTER_ANSI, red, green, blue);
		bool update_iso = wooting_usb_send_feature(WOOTING_SINGLE_COLOR_COMMAND, LED_ENTER_ISO, red, green, blue);

		return update_ansi && update_iso;
	}
	else {
		return wooting_usb_send_feature(WOOTING_SINGLE_COLOR_COMMAND, keyCode, red, green, blue);
	}
}

bool wooting_rgb_direct_reset_key(uint8_t row, uint8_t column) {
	uint8_t keyCode = get_safe_led_idex(row, column);

	if (keyCode == NOLED) {
		return false;
	}
	else if (keyCode == LED_LEFT_SHIFT_ANSI) {
		bool update_ansi = wooting_usb_send_feature(WOOTING_SINGLE_RESET_COMMAND, 0, 0, 0, LED_LEFT_SHIFT_ANSI);
		bool update_iso = wooting_usb_send_feature(WOOTING_SINGLE_RESET_COMMAND, 0, 0, 0, LED_LEFT_SHIFT_ISO);

		return update_ansi && update_iso;
	}
	else if (keyCode == LED_ENTER_ANSI) {
		bool update_ansi = wooting_usb_send_feature(WOOTING_SINGLE_RESET_COMMAND, 0, 0, 0, LED_ENTER_ANSI);
		bool update_iso = wooting_usb_send_feature(WOOTING_SINGLE_RESET_COMMAND, 0, 0, 0, LED_ENTER_ISO);

		return update_ansi && update_iso;
	}
	else {
		return wooting_usb_send_feature(WOOTING_SINGLE_RESET_COMMAND, 0, 0, 0, keyCode);
	}
}

void wooting_rgb_array_auto_update(bool auto_update) {
	wooting_rgb_auto_update = auto_update;
}

bool wooting_rgb_array_update_keyboard() {
	if (rgb_buffer0_changed) {
		if (!wooting_usb_send_buffer(PART0, rgb_buffer0)) {
			return false;
		}
		rgb_buffer0_changed = false;
	}
	
	if (rgb_buffer1_changed) {
		if (!wooting_usb_send_buffer(PART1, rgb_buffer1)) {
			return false;
		}
		rgb_buffer1_changed = false;
	}

	if (rgb_buffer2_changed) {
		if (!wooting_usb_send_buffer(PART2, rgb_buffer2)) {
			return false;
		}
		rgb_buffer2_changed = false;
	}

	if (rgb_buffer3_changed) {
		if (!wooting_usb_send_buffer(PART3, rgb_buffer3)) {
			return false;
		}
		rgb_buffer3_changed = false;
	}

	if (rgb_buffer4_changed) {
		if (!wooting_usb_send_buffer(PART4, rgb_buffer4)) {
			return false;
		}
		rgb_buffer4_changed = false;
	}

	return true;
}

static bool wooting_rgb_array_change_single(uint8_t row, uint8_t column, uint8_t red, uint8_t green, uint8_t blue) {
	const uint8_t pwm_mem_map[48] =
	{
		0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x8, 0x9, 0xa, 0xb, 0xc, 0xd,
		0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d,
		0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d,
		0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d
	};
	uint8_t led_index = get_safe_led_idex(row, column);
	uint8_t *buffer_pointer;

	if (led_index >= 117) {
		return false;
	}
	if (led_index >= 96) {
		buffer_pointer = rgb_buffer4;
		rgb_buffer4_changed = true;
	}
	else if (led_index >= 72) {
		buffer_pointer = rgb_buffer3;
		rgb_buffer3_changed = true;
	}
	else if (led_index >= 48) {
		buffer_pointer = rgb_buffer2;
		rgb_buffer2_changed = true;
	}
	else if (led_index >= 24) {
		buffer_pointer = rgb_buffer1;
		rgb_buffer1_changed = true;
	}
	else {
		buffer_pointer = rgb_buffer0;
		rgb_buffer0_changed = true;
	}
	
	uint8_t buffer_index = pwm_mem_map[led_index % 24];
	buffer_pointer[buffer_index] = red;
	buffer_pointer[buffer_index + 0x10] = green;
	buffer_pointer[buffer_index + 0x20] = blue;

	if (led_index == LED_ENTER_ANSI) {
		uint8_t iso_enter_index = pwm_mem_map[LED_ENTER_ISO - 48];
		rgb_buffer2[iso_enter_index] = rgb_buffer2[buffer_index];
		rgb_buffer2[iso_enter_index + 0x10] = rgb_buffer2[buffer_index + 0x10];
		rgb_buffer2[iso_enter_index + 0x20] = rgb_buffer2[buffer_index + 0x20];
	}
	if (led_index == LED_LEFT_SHIFT_ANSI) {
		uint8_t iso_shift_index = pwm_mem_map[LED_LEFT_SHIFT_ISO];
		rgb_buffer0[iso_shift_index] = rgb_buffer0[buffer_index];
		rgb_buffer0[iso_shift_index + 0x10] = rgb_buffer0[buffer_index + 0x10];
		rgb_buffer0[iso_shift_index + 0x20] = rgb_buffer0[buffer_index + 0x20];
	}

	return true;
}

bool wooting_rgb_array_set_single(uint8_t row, uint8_t column, uint8_t red, uint8_t green, uint8_t blue) {
	if (!wooting_rgb_array_change_single(row, column, red, green, blue)) {
		return false;
	}

	if (wooting_rgb_auto_update) {
		return wooting_rgb_array_update_keyboard();
	}
	else {
		return true;
	}
}

bool wooting_rgb_array_set_full(const uint8_t* colors_buffer) {
	int baseIndex = 0;
	for (int row = 0; row < WOOTING_RGB_ROWS; row++) {
		for (int col = 0; col < WOOTING_RGB_COLS; col++) {
			uint8_t red = colors_buffer[baseIndex + 0];
			uint8_t green = colors_buffer[baseIndex + 1];
			uint8_t blue = colors_buffer[baseIndex + 2];

			wooting_rgb_array_change_single(row, col, red, green, blue);

			baseIndex += 3;
		}
	}

	if (wooting_rgb_auto_update) {
		return wooting_rgb_array_update_keyboard();
	}
	else {
		return true;
	}
}
