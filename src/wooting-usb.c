/*
 * Copyright 2018 Wooting Technologies B.V.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
#include "wooting-usb.h"
#include "wooting-rgb-sdk.h"
#include "hidapi.h"
#include "stdlib.h"
#include "string.h"

#define WOOTING_COMMAND_SIZE 8
#define WOOTING_REPORT_SIZE 128 + 1
#define WOOTING_V2_REPORT_SIZE 256 + 1
#define WOOTING_V1_RESPONSE_SIZE 128
#define WOOTING_V2_RESPONSE_SIZE 256

#define WOOTING_VID 0x03EB
#define WOOTING_VID2 0x31e3

#define WOOTING_ONE_PID 0xFF01
#define WOOTING_ONE_V2_PID 0x1100
// Every keyboard can have an alternative PID for gamepad driver compatibility
// (indicated by X)
#define V2_ALT_PID_0 0x0
#define V2_ALT_PID_1 0x1
#define V2_ALT_PID_2 0x2

#define WOOTING_TWO_PID 0xFF02
#define WOOTING_TWO_V2_PID 0x1200

#define WOOTING_TWO_LE_PID 0x1210

#define WOOTING_TWO_HE_PID 0x1220
#define WOOTING_60HE_PID 0x1300
#define WOOTING_60HE_ARM_PID 0x1310

#define CFG_USAGE_PAGE 0x1337

static WOOTING_USB_META *wooting_usb_meta;

static void_cb disconnected_callback = NULL;
static hid_device *keyboard_handle = NULL;

static WOOTING_USB_META wooting_usb_meta_array[WOOTING_MAX_RGB_DEVICES];
static hid_device *keyboard_handle_array[WOOTING_MAX_RGB_DEVICES];

static uint8_t connected_keyboards = 0;
static bool enumerating = false;

static void debug_print_buffer(uint8_t *buff, size_t len);

static uint16_t getCrc16ccitt(const uint8_t *buffer, uint16_t size) {
  uint16_t crc = 0;

  while (size--) {
    crc ^= (*buffer++ << 8);

    for (uint8_t i = 0; i < 8; ++i) {
      if (crc & 0x8000) {
        crc = (crc << 1) ^ 0x1021;
      } else {
        crc = crc << 1;
      }
    }
  }

  return crc;
}

typedef void (*set_meta_func)();
void walk_hid_devices(struct hid_device_info *hid_info_walker,
                      set_meta_func meta_func);

static void reset_meta(WOOTING_USB_META *device_meta) {
  device_meta->connected = false;

  device_meta->model = "N/A";
  device_meta->device_type = -1;
  device_meta->max_rows = 0;
  device_meta->max_columns = 0;
  device_meta->led_index_max = 0;
  device_meta->v2_interface = false;
  device_meta->layout = LAYOUT_UNKNOWN;
}

static void set_meta_wooting_one(WOOTING_USB_META *device_meta) {
  device_meta->model = "Wooting One";
  device_meta->device_type = DEVICE_KEYBOARD_TKL;
  device_meta->max_rows = WOOTING_RGB_ROWS;
  device_meta->max_columns = WOOTING_ONE_RGB_COLS;
  device_meta->led_index_max = WOOTING_ONE_KEY_CODE_LIMIT;
  device_meta->v2_interface = false;
}

static void set_meta_wooting_one_v2(WOOTING_USB_META *device_meta) {
  set_meta_wooting_one(device_meta);
  device_meta->v2_interface = true;
}

static void set_meta_wooting_two(WOOTING_USB_META *device_meta) {
  device_meta->model = "Wooting Two";
  device_meta->device_type = DEVICE_KEYBOARD;
  device_meta->max_rows = WOOTING_RGB_ROWS;
  device_meta->max_columns = WOOTING_TWO_RGB_COLS;
  device_meta->led_index_max = WOOTING_TWO_KEY_CODE_LIMIT;
  device_meta->v2_interface = false;
}

static void set_meta_wooting_two_v2(WOOTING_USB_META *device_meta) {
  set_meta_wooting_two(device_meta);
  device_meta->v2_interface = true;
}

static void set_meta_wooting_two_le(WOOTING_USB_META *device_meta) {
  device_meta->model = "Wooting Two Lekker Edition";
  device_meta->device_type = DEVICE_KEYBOARD;
  device_meta->max_rows = WOOTING_RGB_ROWS;
  device_meta->max_columns = WOOTING_TWO_RGB_COLS;
  device_meta->led_index_max = WOOTING_TWO_KEY_CODE_LIMIT;
  device_meta->v2_interface = true;
}

static void set_meta_wooting_two_he(WOOTING_USB_META *device_meta) {
  device_meta->model = "Wooting Two HE";
  device_meta->device_type = DEVICE_KEYBOARD;
  device_meta->max_rows = WOOTING_RGB_ROWS;
  device_meta->max_columns = WOOTING_TWO_RGB_COLS;
  device_meta->led_index_max = WOOTING_TWO_KEY_CODE_LIMIT;
  device_meta->v2_interface = true;
}

static void set_meta_wooting_60he(WOOTING_USB_META *device_meta) {
  device_meta->model = "Wooting 60HE";
  device_meta->device_type = DEVICE_KEYBOARD_60;
  device_meta->max_rows = WOOTING_RGB_ROWS;
  device_meta->max_columns = 14;
  device_meta->led_index_max = WOOTING_TWO_KEY_CODE_LIMIT;
  device_meta->v2_interface = true;
}

static void set_meta_wooting_60he_arm(WOOTING_USB_META *device_meta) {
  device_meta->model = "Wooting 60HE (ARM)";
  device_meta->device_type = DEVICE_KEYBOARD_60;
  device_meta->max_rows = WOOTING_RGB_ROWS;
  device_meta->max_columns = 14;
  device_meta->led_index_max = WOOTING_TWO_KEY_CODE_LIMIT;
  device_meta->v2_interface = true;
}

WOOTING_USB_META *wooting_usb_get_meta() {
  // We want to initialise the struct to the default values if it hasn't been
  // set
  if (wooting_usb_meta->model == NULL) {
    reset_meta(wooting_usb_meta);
  }

  return wooting_usb_meta;
}

bool wooting_usb_use_v2_interface(void) {
  return wooting_usb_meta->v2_interface;
}

void wooting_usb_disconnect(bool trigger_cb) {
#ifdef DEBUG_LOG
  printf("Keyboard disconnected\n");
#endif
  for (uint8_t i = 0; i < connected_keyboards; i++) {
    reset_meta(&wooting_usb_meta_array[i]);
    if (keyboard_handle_array[i]) {
      hid_close(keyboard_handle_array[i]);
      keyboard_handle_array[i] = keyboard_handle = NULL;
    }
  }

  if (trigger_cb && disconnected_callback) {
    disconnected_callback();
  }

  connected_keyboards = 0;
}

void wooting_usb_set_disconnected_cb(void_cb cb) { disconnected_callback = cb; }

WOOTING_DEVICE_LAYOUT wooting_usb_get_layout() {
  uint8_t buff[20];
  int result = wooting_usb_send_feature_with_response(
      buff, sizeof(buff), WOOTING_DEVICE_CONFIG_COMMAND, 0, 0, 0, 0);
  if (result != -1) {
    uint8_t index = wooting_usb_use_v2_interface() ? 10 : 9;
    uint8_t layout = buff[index];
#ifdef DEBUG_LOG
    printf("Layout result: %d, %d\n", layout, index);
#endif
    if (layout <= LAYOUT_ISO)
      return (WOOTING_DEVICE_LAYOUT)layout;
    else {
      printf("Unknown device layout found %d\n", layout);
      return LAYOUT_UNKNOWN;
    }
  } else {
    printf(
        "Failed to get device config info for layout detection, result: %d\n",
        result);
  }

  return LAYOUT_UNKNOWN;
}

bool wooting_usb_find_keyboard() {
  if (keyboard_handle || connected_keyboards > 0) {
    // #ifdef DEBUG_LOG
    // printf("Got keyboard handle already\n");
    // #endif
    // If keyboard is disconnected read will return -1
    // https://github.com/signal11/hidapi/issues/55#issuecomment-5307209
    // unsigned char stub = 0;
    // if (hid_read_timeout(keyboard_handle, &stub, 0, 0) != -1) {
    // 	#ifdef DEBUG_LOG
    // 	printf("Keyboard succeeded test read\n");
    // 	#endif
    // 	return true;
    // } else {
    // 	#ifdef DEBUG_LOG
    // 	printf("Keyboard failed test read, disconnecting...\n");
    // 	#endif
    // 	wooting_usb_disconnect(true);
    // }

    // catch handle being empty despite having found keyboards
    if (!keyboard_handle)
      wooting_usb_select_device(0);
    return true;
  } else {
#ifdef DEBUG_LOG
    printf("No keyboard handle already\n");
#endif
  }

  // Initilize arrays to default values and allocate memory
  for (uint8_t i = 0; i < WOOTING_MAX_RGB_DEVICES; i++) {
    keyboard_handle_array[i] = NULL;
  }

  // Make sure pointers are the first element in the array
  keyboard_handle = keyboard_handle_array[0];
  wooting_usb_meta = &wooting_usb_meta_array[0];
  reset_meta(wooting_usb_meta);

  struct hid_device_info *hid_info;

  // Set enumerating flag
  enumerating = true;

#define PID_ALT_CHECK(base_pid)                                                \
  (hid_info = hid_enumerate(WOOTING_VID2, base_pid | V2_ALT_PID_0)) != NULL || \
      (hid_info = hid_enumerate(WOOTING_VID2, base_pid | V2_ALT_PID_1)) !=     \
          NULL ||                                                              \
      (hid_info = hid_enumerate(WOOTING_VID2, base_pid | V2_ALT_PID_2)) !=     \
          NULL

  if ((hid_info = hid_enumerate(WOOTING_VID, WOOTING_ONE_PID)) != NULL) {
#ifdef DEBUG_LOG
    printf("Enumerate on Wooting One Successful\n");
#endif
    walk_hid_devices(hid_info, set_meta_wooting_one);
  }
  if (PID_ALT_CHECK(WOOTING_ONE_V2_PID)) {
#ifdef DEBUG_LOG
    printf("Enumerate on Wooting One (V2) Successful\n");
#endif
    walk_hid_devices(hid_info, set_meta_wooting_one_v2);
  }
  if ((hid_info = hid_enumerate(WOOTING_VID, WOOTING_TWO_PID)) != NULL) {
#ifdef DEBUG_LOG
    printf("Enumerate on Wooting Two Successful\n");
#endif
    walk_hid_devices(hid_info, set_meta_wooting_two);
  }
  if (PID_ALT_CHECK(WOOTING_TWO_V2_PID)) {
#ifdef DEBUG_LOG
    printf("Enumerate on Wooting Two (V2) Successful\n");
#endif
    walk_hid_devices(hid_info, set_meta_wooting_two_v2);
  }
  if (PID_ALT_CHECK(WOOTING_TWO_LE_PID)) {
#ifdef DEBUG_LOG
    printf("Enumerate on Wooting Two Lekker Edition Successful\n");
#endif
    walk_hid_devices(hid_info, set_meta_wooting_two_le);
  }
  if (PID_ALT_CHECK(WOOTING_TWO_HE_PID)) {
#ifdef DEBUG_LOG
    printf("Enumerate on Wooting Two HE Successful\n");
#endif
    walk_hid_devices(hid_info, set_meta_wooting_two_he);
  }
  if (PID_ALT_CHECK(WOOTING_60HE_PID)) {
#ifdef DEBUG_LOG
    printf("Enumerate on Wooting 60HE Successful\n");
#endif
    walk_hid_devices(hid_info, set_meta_wooting_60he);
  } else if (PID_ALT_CHECK(WOOTING_60HE_ARM_PID)) {
#ifdef DEBUG_LOG
    printf("Enumerate on Wooting 60HE (ARM) Successful\n");
#endif
    walk_hid_devices(hid_info, set_meta_wooting_60he_arm);
  }

  enumerating = false;

  if (connected_keyboards == 0) {
#ifdef DEBUG_LOG
    printf("Enumerate failed\n");
#endif
    return false;
  }

  // Set first found device as default after hid walking
  wooting_usb_select_device(0);

#ifdef DEBUG_LOG
  printf("Finished looking for keyboards returned: %d\n", connected_keyboards);
#endif
  return connected_keyboards > 0;
}

void walk_hid_devices(struct hid_device_info *hid_info_walker,
                      set_meta_func meta_func) {
  // We can just search for the interface with matching custom Wooting Cfg usage
  // page
  while (hid_info_walker) {
    if (connected_keyboards == WOOTING_MAX_RGB_DEVICES)
      break;
#ifdef DEBUG_LOG
    printf("Found interface No: %d\n", hid_info_walker->interface_number);
    printf("Found usage page: %d\n", hid_info_walker->usage_page);
#endif
    if (hid_info_walker->usage_page == CFG_USAGE_PAGE) {
#ifdef DEBUG_LOG
      printf("Attempting to open\n");
#endif
      keyboard_handle = hid_open_path(hid_info_walker->path);
      if (keyboard_handle) {
#ifdef DEBUG_LOG
        printf("Found keyboard_handle: %s\n", hid_info_walker->path);
        printf("Opened handle: %p\n", keyboard_handle);
#endif

        // Update pointer array and meta
        keyboard_handle_array[connected_keyboards] = keyboard_handle;
        meta_func(&wooting_usb_meta_array[connected_keyboards]);
        (&wooting_usb_meta_array[connected_keyboards])->connected = true;

        // Any feature sends need to be done after the meta is set so the
        // correct value for v2_interface is set

        // Once the keyboard is found send an init command
#ifdef DEBUG_LOG
        bool result =
#endif
            wooting_usb_send_feature(WOOTING_COLOR_INIT_COMMAND, 0, 0, 0, 0);
#ifdef DEBUG_LOG
        printf("Color init result: %d\n", result);
#endif

        (&wooting_usb_meta_array[connected_keyboards])->layout =
            wooting_usb_get_layout();

        // Increment found keyboard count and switch to the next element in the
        // array
        connected_keyboards++;
      } else {
#ifdef DEBUG_LOG
        printf("No Keyboard handle: %S\n", hid_error(NULL));
#endif
      }
    }
    hid_info_walker = hid_info_walker->next;
  }

  hid_free_enumeration(hid_info_walker);
}

bool wooting_usb_select_device(uint8_t device_index) {
  // Only change device if the given index is valid
  if (device_index < 0 || device_index >= WOOTING_MAX_RGB_DEVICES ||
      (device_index >= connected_keyboards && !enumerating))
    return false;

  // Fetch pointer and meta data from arrays
  keyboard_handle = keyboard_handle_array[device_index];
  wooting_usb_meta = &wooting_usb_meta_array[device_index];
  // Initilize meta data should it somehow be empty
  if (wooting_usb_meta->model == NULL)
    reset_meta(wooting_usb_meta);

  wooting_rgb_select_buffer(device_index);

#ifdef DEBUG_LOG
  printf("Keyboard handle: %p | Model: %s\n", keyboard_handle,
         wooting_usb_meta->model);
#endif

  return true;
}

WOOTING_USB_META *wooting_usb_get_device_meta(uint8_t device_index) {
  // Only change device if the given index is valid
  if (device_index < 0 || device_index >= WOOTING_MAX_RGB_DEVICES ||
      (device_index >= connected_keyboards && !enumerating))
    return NULL;

  // Fetch pointer and meta data from arrays
  return &wooting_usb_meta_array[device_index];
}

uint8_t wooting_usb_device_count() { return connected_keyboards; }

bool wooting_usb_send_buffer_v1(RGB_PARTS part_number, uint8_t rgb_buffer[]) {
  if (!wooting_usb_find_keyboard()) {
    return false;
  }

  uint8_t report_buffer[WOOTING_REPORT_SIZE] = {0};

  report_buffer[0] = 0;                         // HID report index (unused)
  report_buffer[1] = 0xD0;                      // Magicword
  report_buffer[2] = 0xDA;                      // Magicword
  report_buffer[3] = WOOTING_RAW_COLORS_REPORT; // Report ID
  switch (part_number) {
  case PART0: {
    report_buffer[4] = 0; // Slave nr
    report_buffer[5] = 0; // Reg start address
    break;
  }
  case PART1: {
    report_buffer[4] = 0;                   // Slave nr
    report_buffer[5] = RGB_RAW_BUFFER_SIZE; // Reg start address
    break;
  }
  case PART2: {
    report_buffer[4] = 1; // Slave nr
    report_buffer[5] = 0; // Reg start address
    break;
  }
  case PART3: {
    report_buffer[4] = 1;                   // Slave nr
    report_buffer[5] = RGB_RAW_BUFFER_SIZE; // Reg start address
    break;
  }
  // wooting_rgb_array_update_keyboard will not run into this
  case PART4: {
    // Wooting One will not have this part of the report
    if (wooting_usb_meta->device_type != DEVICE_KEYBOARD) {
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

  uint16_t crc =
      getCrc16ccitt((uint8_t *)&report_buffer, WOOTING_REPORT_SIZE - 2);
  report_buffer[127] = (uint8_t)crc;
  report_buffer[128] = crc >> 8;
  int report_size =
      hid_write(keyboard_handle, report_buffer, WOOTING_REPORT_SIZE);
  if (report_size == WOOTING_REPORT_SIZE) {
    return true;
  } else {
#ifdef DEBUG_LOG
    printf("Got report size: %d, expected: %d, disconnecting..\n", report_size,
           WOOTING_REPORT_SIZE);
#endif
    wooting_usb_disconnect(true);
    return false;
  }
}

bool wooting_usb_send_buffer_v2(
    uint16_t rgb_buffer[WOOTING_RGB_ROWS][WOOTING_RGB_COLS]) {
  if (!wooting_usb_find_keyboard()) {
    return false;
  }

  uint8_t report_buffer[WOOTING_V2_REPORT_SIZE] = {0};
  report_buffer[0] = 0;                         // HID report index (unused)
  report_buffer[1] = 0xD0;                      // Magicword
  report_buffer[2] = 0xDA;                      // Magicword
  report_buffer[3] = WOOTING_RAW_COLORS_REPORT; // Report ID
  memcpy(&report_buffer[4], rgb_buffer,
         WOOTING_RGB_ROWS * WOOTING_RGB_COLS * sizeof(uint16_t));

  int report_size =
      hid_write(keyboard_handle, report_buffer, WOOTING_V2_REPORT_SIZE);
  if (report_size == WOOTING_V2_REPORT_SIZE) {
#ifdef DEBUG_LOG
    printf("Successfully sent V2 buffer...\n");
#endif
    return true;
  } else if (report_size == 65) {
    for (uint8_t i = 0; i < 3; i++) {
      uint8_t child_buff[65] = {0};
      memcpy(&child_buff[1], &report_buffer[((i + 1) * 64) + 1], 64);
      int child_report = hid_write(keyboard_handle, child_buff, 65);
#ifdef DEBUG_LOG
      printf("Child report size: %d\n", child_report)
#endif
    }

  } else {
#ifdef DEBUG_LOG
    printf("Got report size: %d, expected: %d, disconnecting..\n", report_size,
           WOOTING_V2_REPORT_SIZE);
#endif
    wooting_usb_disconnect(true);
    return false;
  }
}

int wooting_usb_send_feature_buff(uint8_t commandId, uint8_t parameter0,
                                  uint8_t parameter1, uint8_t parameter2,
                                  uint8_t parameter3) {
  uint8_t report_buffer[WOOTING_COMMAND_SIZE];

  report_buffer[0] = 0;    // HID report index (unused)
  report_buffer[1] = 0xD0; // Magic word
  report_buffer[2] = 0xDA; // Magic word
  report_buffer[3] = commandId;
  report_buffer[4] = parameter3;
  report_buffer[5] = parameter2;
  report_buffer[6] = parameter1;
  report_buffer[7] = parameter0;

  return hid_send_feature_report(keyboard_handle, report_buffer,
                                 WOOTING_COMMAND_SIZE);
}

size_t wooting_usb_get_response_size(void) {
  if (wooting_usb_use_v2_interface()) {
    return WOOTING_V2_RESPONSE_SIZE;
  } else {
    return WOOTING_V1_RESPONSE_SIZE;
  }
}

bool wooting_usb_send_feature(uint8_t commandId, uint8_t parameter0,
                              uint8_t parameter1, uint8_t parameter2,
                              uint8_t parameter3) {
  if (!wooting_usb_find_keyboard()) {
    return false;
  }

#ifdef DEBUG_LOG
  printf("Sending feature: %d\n", commandId);
#endif

  int command_size = wooting_usb_send_feature_buff(
      commandId, parameter0, parameter1, parameter2, parameter3);
  size_t response_size = wooting_usb_get_response_size();

  // Just read the response and discard it
  uint8_t *buff = (uint8_t *)calloc(response_size, sizeof(uint8_t));
  int result = wooting_usb_read_response(buff, response_size);
  free(buff);
#ifdef DEBUG_LOG
  printf("Read result %d \n", result);
#endif

  if (command_size == WOOTING_COMMAND_SIZE && result == response_size) {
    return true;
  } else {
#ifdef DEBUG_LOG
    printf("Got command size: %d, expected: %d, disconnecting..\n",
           command_size, WOOTING_COMMAND_SIZE);
#endif

    wooting_usb_disconnect(true);
    return false;
  }
}

int wooting_usb_send_feature_with_response(
    uint8_t *buff, size_t len, uint8_t commandId, uint8_t parameter0,
    uint8_t parameter1, uint8_t parameter2, uint8_t parameter3) {
  if (!wooting_usb_find_keyboard()) {
    return -1;
  }

#ifdef DEBUG_LOG
  printf("Sending feature with response: %d\n", commandId);
#endif

  int command_size = wooting_usb_send_feature_buff(
      commandId, parameter0, parameter1, parameter2, parameter3);
  if (command_size == WOOTING_COMMAND_SIZE) {
    size_t response_size = wooting_usb_get_response_size();
    uint8_t *responseBuff = (uint8_t *)calloc(response_size, sizeof(uint8_t));

    int result = wooting_usb_read_response(responseBuff, response_size);

    if (result == response_size) {
      memcpy(buff, responseBuff, len);
      free(responseBuff);
      return result;
    } else {
#ifdef DEBUG_LOG
      printf("Got response size: %d, expected: %d, disconnecting..\n", result,
             (int)response_size);
#endif

      free(responseBuff);
      wooting_usb_disconnect(true);
      return -1;
    }
  } else {
#ifdef DEBUG_LOG
    printf("Got command size: %d, expected: %d, disconnecting..\n",
           command_size, WOOTING_COMMAND_SIZE);
#endif

    wooting_usb_disconnect(true);
    return false;
  }
}

static void debug_print_buffer(uint8_t *buff, size_t len) {
#ifdef DEBUG_LOG
  printf("Buffer content \n");
  for (int i = 0; i < len; i++) {
    printf("%d%s", buff[i], i < len - 1 ? ", " : "");
  }
  printf("\n");
#endif
}

int wooting_usb_read_response_timeout(uint8_t *buff, size_t len,
                                      int milliseconds) {
  int result = hid_read_timeout(keyboard_handle, buff, len, milliseconds);
  while (result < len) {
    result += hid_read_timeout(keyboard_handle, buff + result, len - result,
                               milliseconds);
  }
#ifdef DEBUG_LOG
  printf("hid_read_timeout result code: %d\n", result);
#endif
  debug_print_buffer(buff, len);
  return result;
}

int wooting_usb_read_response(uint8_t *buff, size_t len) {
  int result = hid_read(keyboard_handle, buff, len);
  while (result < len) {
    result += hid_read(keyboard_handle, buff + result, len - result);
  }
#ifdef DEBUG_LOG
  printf("hid_read result code: %d\n", result);
#endif
  debug_print_buffer(buff, len);
  return result;
}
