/*
* Copyright 2018 Wooting Technologies B.V.
*
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
#pragma once

typedef void(*void_cb)(void);

typedef enum RGB_PARTS {
	PART0,
	PART1,
	PART2,
	PART3,
	PART4
} RGB_PARTS;

#define RGB_RAW_BUFFER_SIZE 96

#define WOOTING_RAW_COLORS_REPORT 11
#define WOOTING_SINGLE_COLOR_COMMAND 30
#define WOOTING_SINGLE_RESET_COMMAND 31
#define WOOTING_RESET_ALL_COMMAND 32
#define WOOTING_COLOR_INIT_COMMAND 33

void wooting_usb_set_disconnected_cb(void_cb cb);
bool wooting_usb_find_keyboard();
bool wooting_usb_send_buffer(RGB_PARTS part_number, uint8_t rgb_buffer[]);
bool wooting_usb_send_feature(uint8_t commandId, uint8_t parameter0, uint8_t parameter1, uint8_t parameter2, uint8_t parameter3);
