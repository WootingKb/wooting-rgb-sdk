/*
Copyright 2018 Wooting Technologies B.V.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http ://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
-
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
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
