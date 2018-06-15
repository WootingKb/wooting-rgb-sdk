#include "stdbool.h"
#include "stdint.h"
#include "hidapi.h"
#include "wooting-rgb-control.h"

#define NOLED 50
#define LED_LEFT_SHIFT_ANSI 9
#define LED_LEFT_SHIFT_ISO 7
#define LED_ENTER_ANSI 65
#define LED_ENTER_ISO 62

#define RGB_RAW_BUFFER_SIZE 96
#define WOOTING_COMMAND_SIZE 8
#define WOOTING_REPORT_SIZE 129
#define WOOTING_REPORT_BODY_SIZE
#define WOOTING_ONE_VID 0x03EB
#define WOOTING_ONE_PID 0xFF01
#define WOOTING_ONE_CONFIG_USAGE_PAGE 0x1337

#define WOOTING_RAW_COLORS_REPORT 11
#define WOOTING_SINGLE_COLOR_COMMAND 30
#define WOOTING_SINGLE_RESET_COMMAND 31
#define WOOTING_RESET_ALL_COMMAND 32
#define WOOTING_COLOR_INIT_COMMAND 33

typedef enum RGB_PARTS {
	PART0,
	PART1,
	PART2,
	PART3,
	PART4
} RGB_PARTS;

static hid_device* keyboard_handle = NULL;
static void_cb disconnected_callback = NULL;
static bool wooting_rgb_auto_update = false;

static uint8_t get_safe_led_idex(uint8_t row, uint8_t column);
static uint16_t getCrc16ccitt(const uint8_t* buffer, uint16_t size);
static void wooting_keyboard_disconnected();
static bool wooting_find_keyboard();
static bool wooting_send_buffer(RGB_PARTS part_number, uint8_t rgb_buffer[]);
static bool wooting_rgb_send_feature(uint8_t commandId, uint8_t parameter0, uint8_t parameter1, uint8_t parameter2, uint8_t parameter3);

static uint8_t wooting_rgb_array[WOOTING_RGB_ROWS][WOOTING_RGB_COLS][3] = {
	{
		{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 }
	},
	{
		{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 }
	},
	{
		{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 }
	},
	{
		{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 }
	},
	{
		{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 }
	},
	{
		{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 },{ 0, 255, 255 }
	}
};

static uint8_t get_safe_led_idex(uint8_t row, uint8_t column) {
	static uint8_t rgb_led_index[WOOTING_RGB_ROWS][WOOTING_RGB_COLS] = {
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

static void wooting_keyboard_disconnected() {
	hid_close(keyboard_handle);
	keyboard_handle = NULL;

	if (disconnected_callback) {
		disconnected_callback();
	}
}

static bool wooting_find_keyboard() {
	if (keyboard_handle) {
		return true;
	}
	else {
		struct hid_device_info* hid_info = hid_enumerate(WOOTING_ONE_VID, WOOTING_ONE_PID);

		if (hid_info == NULL) {
			return false;
		}

		// Loop through linked list of hid_info untill the analog interface is found
		while (hid_info) {
			if (hid_info->usage_page != WOOTING_ONE_CONFIG_USAGE_PAGE) {
				hid_info = hid_info->next;
			} else {
				keyboard_handle = hid_open_path(hid_info->path);

				// Once the keyboard is found send an init command and abuse two reads to make a 50 ms delay to make sure the keyboard is ready
				wooting_rgb_send_feature(WOOTING_COLOR_INIT_COMMAND, 0, 0, 0, 0);
				unsigned char *stub = NULL;
				hid_read(keyboard_handle, stub, 0);
				hid_read_timeout(keyboard_handle, stub, 0, 50);

				return true;
			}
		}

		// Analog interface not found
		return false;
	}
}

static bool wooting_send_buffer(RGB_PARTS part_number, uint8_t rgb_buffer[]) {
	if (!wooting_find_keyboard()) {
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
		case PART4: {
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
	} else {
		wooting_keyboard_disconnected();
		return false;
	}
}

static bool wooting_rgb_send_feature(uint8_t commandId, uint8_t parameter0, uint8_t parameter1, uint8_t parameter2, uint8_t parameter3) {
	if (!wooting_find_keyboard()) {
		return false;
	}

	uint8_t report_buffer[WOOTING_COMMAND_SIZE];

	report_buffer[0] = 0;
	report_buffer[1] = 0xD0;
	report_buffer[2] = 0xDA;
	report_buffer[3] = commandId;
	report_buffer[4] = parameter3;
	report_buffer[5] = parameter2;
	report_buffer[6] = parameter1;
	report_buffer[7] = parameter0;

	if (hid_send_feature_report(keyboard_handle, report_buffer, WOOTING_COMMAND_SIZE) == WOOTING_COMMAND_SIZE) {
		return true;
	}
	else {
		wooting_keyboard_disconnected();
		return false;
	}
}

bool wooting_rgb_kbd_connected() {
	return hid_enumerate(WOOTING_ONE_VID, WOOTING_ONE_PID) != NULL ? true : false;
}

void wooting_rgb_set_disconnected_cb(void_cb cb) {
	disconnected_callback = cb;
}

bool wooting_rgb_reset() {
	return wooting_rgb_send_feature(WOOTING_RESET_ALL_COMMAND, 0, 0, 0, 0);
}

bool wooting_rgb_direct_set_key(uint8_t row, uint8_t column, uint8_t red, uint8_t green, uint8_t blue) {
	uint8_t keyCode = get_safe_led_idex(row, column);

	if (keyCode == NOLED) {
		return false;
	}
	else if (keyCode == LED_LEFT_SHIFT_ANSI) {
		bool update_ansi = wooting_rgb_send_feature(WOOTING_SINGLE_COLOR_COMMAND, LED_LEFT_SHIFT_ANSI, red, green, blue);
		bool update_iso = wooting_rgb_send_feature(WOOTING_SINGLE_COLOR_COMMAND, LED_LEFT_SHIFT_ISO, red, green, blue);

		return update_ansi && update_iso;
	}
	else if (keyCode == LED_ENTER_ANSI) {
		bool update_ansi = wooting_rgb_send_feature(WOOTING_SINGLE_COLOR_COMMAND, LED_ENTER_ANSI, red, green, blue);
		bool update_iso = wooting_rgb_send_feature(WOOTING_SINGLE_COLOR_COMMAND, LED_ENTER_ISO, red, green, blue);

		return update_ansi && update_iso;
	}
	else {
		return wooting_rgb_send_feature(WOOTING_SINGLE_COLOR_COMMAND, keyCode, red, green, blue);
	}
}

bool wooting_rgb_direct_reset_key(uint8_t row, uint8_t column) {
	uint8_t keyCode = get_safe_led_idex(row, column);

	if (keyCode == NOLED) {
		return true;
	}
	else if (keyCode == LED_LEFT_SHIFT_ANSI) {
		bool update_ansi = wooting_rgb_send_feature(WOOTING_SINGLE_RESET_COMMAND, 0, 0, 0, LED_LEFT_SHIFT_ANSI);
		bool update_iso = wooting_rgb_send_feature(WOOTING_SINGLE_RESET_COMMAND, 0, 0, 0, LED_LEFT_SHIFT_ISO);

		return update_ansi && update_iso;
	}
	else if (keyCode == LED_ENTER_ANSI) {
		bool update_ansi = wooting_rgb_send_feature(WOOTING_SINGLE_RESET_COMMAND, 0, 0, 0, LED_ENTER_ANSI);
		bool update_iso = wooting_rgb_send_feature(WOOTING_SINGLE_RESET_COMMAND, 0, 0, 0, LED_ENTER_ISO);

		return update_ansi && update_iso;
	}
	else {
		return wooting_rgb_send_feature(WOOTING_SINGLE_RESET_COMMAND, 0, 0, 0, keyCode);
	}
}

bool wooting_rgb_array_update_keyboard() {
	const uint8_t pwm_mem_map[48] =
	{
		0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x8, 0x9, 0xa, 0xb, 0xc, 0xd,
		0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d,
		0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d,
		0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d
	};

	uint8_t rgb_buffer0[RGB_RAW_BUFFER_SIZE] = { 0 };
	uint8_t rgb_buffer1[RGB_RAW_BUFFER_SIZE] = { 0 };
	uint8_t rgb_buffer2[RGB_RAW_BUFFER_SIZE] = { 0 };
	uint8_t rgb_buffer3[RGB_RAW_BUFFER_SIZE] = { 0 };

	for (int row = 0; row < WOOTING_RGB_ROWS; row++) {
		for (int col = 0; col < WOOTING_RGB_COLS; col++) {
			uint8_t led_index = get_safe_led_idex(row, col);

			if (led_index >= 96) {
				break;
			}

			if (led_index >= 72) {
				uint8_t buffer_index = pwm_mem_map[led_index - 72];
				rgb_buffer3[buffer_index] = wooting_rgb_array[row][col][0];
				rgb_buffer3[buffer_index + 0x10] = wooting_rgb_array[row][col][1];
				rgb_buffer3[buffer_index + 0x20] = wooting_rgb_array[row][col][2];
			}
			else if (led_index >= 48) {
				uint8_t buffer_index = pwm_mem_map[led_index - 48];
				rgb_buffer2[buffer_index] = wooting_rgb_array[row][col][0];
				rgb_buffer2[buffer_index + 0x10] = wooting_rgb_array[row][col][1];
				rgb_buffer2[buffer_index + 0x20] = wooting_rgb_array[row][col][2];

				if (led_index == LED_ENTER_ANSI) {
					uint8_t iso_enter_index = pwm_mem_map[LED_ENTER_ISO-48];
					rgb_buffer2[iso_enter_index] = rgb_buffer2[buffer_index];
					rgb_buffer2[iso_enter_index + 0x10] = rgb_buffer2[buffer_index + 0x10];
					rgb_buffer2[iso_enter_index + 0x20] = rgb_buffer2[buffer_index + 0x20];
				}
			}
			else if (led_index >= 24) {
				uint8_t buffer_index = pwm_mem_map[led_index - 24];
				rgb_buffer1[buffer_index] = wooting_rgb_array[row][col][0];
				rgb_buffer1[buffer_index + 0x10] = wooting_rgb_array[row][col][1];
				rgb_buffer1[buffer_index + 0x20] = wooting_rgb_array[row][col][2];
			}
			else {
				uint8_t buffer_index = pwm_mem_map[led_index];
				rgb_buffer0[buffer_index] = wooting_rgb_array[row][col][0];
				rgb_buffer0[buffer_index + 0x10] = wooting_rgb_array[row][col][1];
				rgb_buffer0[buffer_index + 0x20] = wooting_rgb_array[row][col][2];

				if (led_index == LED_LEFT_SHIFT_ANSI) {
					uint8_t iso_shift_index = pwm_mem_map[LED_LEFT_SHIFT_ISO];
					rgb_buffer0[iso_shift_index] = rgb_buffer0[buffer_index];
					rgb_buffer0[iso_shift_index + 0x10] = rgb_buffer0[buffer_index + 0x10];
					rgb_buffer0[iso_shift_index + 0x20] = rgb_buffer0[buffer_index + 0x20];
				}
			}
		}
	}

	if (!wooting_send_buffer(PART0, rgb_buffer0)) {
		return false;
	}
	if (!wooting_send_buffer(PART1, rgb_buffer1)) {
		return false;
	}
	if (!wooting_send_buffer(PART2, rgb_buffer2)) {
		return false;
	}
	if (!wooting_send_buffer(PART3, rgb_buffer3)) {
		return false;
	}

	return true;
}

void wooting_rgb_array_auto_update(bool auto_update) {
	wooting_rgb_auto_update = auto_update;
}

bool wooting_rgb_array_set_single(uint8_t row, uint8_t column, uint8_t red, uint8_t green, uint8_t blue) {
	if (row >= WOOTING_RGB_ROWS || column >= WOOTING_RGB_COLS) {
		return false;
	}
	
	wooting_rgb_array[row][column][0] = red;
	wooting_rgb_array[row][column][1] = green;
	wooting_rgb_array[row][column][2] = blue;

	if (wooting_rgb_auto_update) {
		return wooting_rgb_array_update_keyboard();
	}
	else {
		return true;
	}
}

bool wooting_rgb_array_set_full(uint8_t *colors_buffer) {
	memcpy(wooting_rgb_array, colors_buffer, sizeof(wooting_rgb_array));

	if (wooting_rgb_auto_update) {
		return wooting_rgb_array_update_keyboard();
	}
	else {
		return true;
	}
}
