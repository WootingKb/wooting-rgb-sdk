/*
* Copyright 2018 Wooting Technologies B.V.
*
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/
#include "stdint.h"
#include "stdbool.h"
#include "string.h"
#include "wooting-usb.h"
#include "hidapi.h"

#define WOOTING_COMMAND_SIZE 8
#define WOOTING_REPORT_SIZE 129
#define WOOTING_VID 0x03EB
#define WOOTING_ONE_PID 0xFF01
#define WOOTING_TWO_PID 0xFF02

static uint16_t getCrc16ccitt(const uint8_t* buffer, uint16_t size);

static void_cb disconnected_callback = NULL;
static hid_device* keyboard_handle = NULL;

static bool is_wooting_one = false;
static uint8_t wooting_key_code_limit;

static uint16_t getCrc16ccitt(const uint8_t* buffer, uint16_t size)
{
	uint16_t crc = 0;

	while (size--) {
		crc ^= (*buffer++ << 8);

		for (uint8_t i = 0; i < 8; ++i) {
			if (crc & 0x8000) {
				crc = (crc << 1) ^ 0x1021;
			}
			else {
				crc = crc << 1;
			}
		}
	}

	return crc;
}

void wooting_usb_disconnect(bool trigger_cb) {
	hid_close(keyboard_handle);
	keyboard_handle = NULL;

	if (trigger_cb && disconnected_callback) {
		disconnected_callback();
	}
}

void wooting_usb_set_disconnected_cb(void_cb cb) {
	disconnected_callback = cb;
}

bool wooting_usb_is_wooting_one() {
	return is_wooting_one;
}

uint8_t get_wooting_key_code_limit() {
	return wooting_key_code_limit;
}

bool wooting_usb_find_keyboard() {
	if (keyboard_handle) {
		// If keyboard is disconnected read will return -1
		// https://github.com/signal11/hidapi/issues/55#issuecomment-5307209
		unsigned char stub = 0;
		return hid_read_timeout(keyboard_handle, &stub, 0, 0) != -1;
	}
	
	struct hid_device_info* hid_info;
	
	if ((hid_info = hid_enumerate(WOOTING_VID, WOOTING_ONE_PID)) != NULL) {
		is_wooting_one = true;
		wooting_key_code_limit = WOOTING_ONE_KEY_CODE_LIMIT;
	}
	else if ((hid_info = hid_enumerate(WOOTING_VID, WOOTING_TWO_PID)) != NULL) {
		is_wooting_one = false;
		wooting_key_code_limit = WOOTING_TWO_KEY_CODE_LIMIT;
	}
	else {
		return false;
	}

	bool keyboard_found = false;

	// The amount of interfaces is variable, so we need to look for the configuration interface
	// In the Wooting one keyboard the configuration interface is always 4 lower than the highest number
	struct hid_device_info* hid_info_walker = hid_info;
	uint8_t highestInterfaceNr = 0;
	while (hid_info_walker) {
		if (hid_info_walker->interface_number > highestInterfaceNr) {
			highestInterfaceNr = hid_info_walker->interface_number;
		}
		hid_info_walker = hid_info_walker->next;
	}

	uint8_t interfaceNr = highestInterfaceNr - 4;

	// Reset walker and look for the interface number
	hid_info_walker = hid_info;
	while (hid_info_walker) {
		if (hid_info_walker->interface_number == interfaceNr) {
			keyboard_handle = hid_open_path(hid_info_walker->path);

			if (keyboard_handle) {
				// Once the keyboard is found send an init command and abuse two reads to make a 50 ms delay to make sure the keyboard is ready
				wooting_usb_send_feature(WOOTING_COLOR_INIT_COMMAND, 0, 0, 0, 0);
				unsigned char stub = 0;
				hid_read(keyboard_handle, &stub, 0);
				hid_read_timeout(keyboard_handle, &stub, 0, 50);

				keyboard_found = true;
			}
			break;
		}
	
		hid_info_walker = hid_info_walker->next;
	}

	hid_free_enumeration(hid_info);
	return keyboard_found;
}

bool wooting_usb_send_buffer(RGB_PARTS part_number, uint8_t rgb_buffer[]) {
	if (!wooting_usb_find_keyboard()) {
		return false;
	}

	uint8_t report_buffer[WOOTING_REPORT_SIZE] = { 0 };

	report_buffer[0] = 0; // HID report index (unused)
	report_buffer[1] = 0xD0; // Magicword
	report_buffer[2] = 0xDA; // Magicword
	report_buffer[3] = WOOTING_RAW_COLORS_REPORT; // Report ID
	switch (part_number) {
	case PART0: {
		report_buffer[4] = 0; // Slave nr
		report_buffer[5] = 0; // Reg start address
		break;
	}
	case PART1: {
		report_buffer[4] = 0; // Slave nr
		report_buffer[5] = RGB_RAW_BUFFER_SIZE; // Reg start address
		break;
	}
	case PART2: {
		report_buffer[4] = 1; // Slave nr
		report_buffer[5] = 0; // Reg start address
		break;
	}
	case PART3: {
		report_buffer[4] = 1; // Slave nr
		report_buffer[5] = RGB_RAW_BUFFER_SIZE; // Reg start address
		break;
	}
	// wooting_rgb_array_update_keyboard will not run into this
	case PART4: {
		if (is_wooting_one) {
			return false;
		}
		report_buffer[4] = 2; // Slave nr
		report_buffer[5] = 0; // Reg start address
		break;
	}
	default: {
		return false;
	}
	}

	memcpy(&report_buffer[6], rgb_buffer, RGB_RAW_BUFFER_SIZE);

	uint16_t crc = getCrc16ccitt((uint8_t*)&report_buffer, WOOTING_REPORT_SIZE - 2);
	report_buffer[127] = (uint8_t)crc;
	report_buffer[128] = crc >> 8;

	if (hid_write(keyboard_handle, report_buffer, WOOTING_REPORT_SIZE) == WOOTING_REPORT_SIZE) {
		return true;
	}
	else {
		wooting_usb_disconnect(true);
		return false;
	}
}

bool wooting_usb_send_feature(uint8_t commandId, uint8_t parameter0, uint8_t parameter1, uint8_t parameter2, uint8_t parameter3) {
	if (!wooting_usb_find_keyboard()) {
		return false;
	}
	// prevent sending unnecessary data to the wootings if the index exceedes it's capabilities
	if ((commandId == WOOTING_SINGLE_COLOR_COMMAND && parameter0 > wooting_key_code_limit) || (commandId == WOOTING_SINGLE_RESET_COMMAND && parameter3 > wooting_key_code_limit)) {
		// this is not a usb error so let's return true. wooting_rgb_direct_set_key would also behave differently otherwise.
		return true;
	}

	uint8_t report_buffer[WOOTING_COMMAND_SIZE];

	report_buffer[0] = 0; // HID report index (unused)
	report_buffer[1] = 0xD0; // Magic word
	report_buffer[2] = 0xDA; // Magic word
	report_buffer[3] = commandId;
	report_buffer[4] = parameter3;
	report_buffer[5] = parameter2;
	report_buffer[6] = parameter1;
	report_buffer[7] = parameter0;

	if (hid_send_feature_report(keyboard_handle, report_buffer, WOOTING_COMMAND_SIZE) == WOOTING_COMMAND_SIZE) {
		return true;
	}
	else {
		wooting_usb_disconnect(true);
		return false;
	}
}
