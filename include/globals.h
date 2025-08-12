#ifndef GLOBALS_H
#define GLOBALS_H

#include <Arduino.h>
#include "SSD1306.h"
#include "esp_camera.h"
#include <BLEDevice.h>
#include <Preferences.h>
#include "esp_sleep.h" // Added for light sleep

// --- PROTOCOL & BATCH CONFIGURATION ---
#define CHUNK_SIZE 512
#define IMAGE_BATCH_SIZE 20 // Maximum number of images we can buffer in PSRAM
#ifndef BLE_DEVICE_NAME
#define BLE_DEVICE_NAME "T-Camera-BLE-Batch"
#endif

// --- BLE UUIDs ---
#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID_STATUS "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define CHARACTERISTIC_UUID_DATA "7347e350-5552-4822-8243-b8923a4114d2"
#define CHARACTERISTIC_UUID_COMMAND "a244c201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID_CONFIG "a31a6820-8437-4f55-8898-5226c04a29a3"

// --- Pin Definitions (Unchanged) ---
#define PWDN_GPIO_NUM -1
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 4
#define SIOD_GPIO_NUM 18
#define SIOC_GPIO_NUM 23
#define Y9_GPIO_NUM 36
#define Y8_GPIO_NUM 37
#define Y7_GPIO_NUM 38
#define Y6_GPIO_NUM 39
#define Y5_GPIO_NUM 35
#define Y4_GPIO_NUM 14
#define Y3_GPIO_NUM 13
#define Y2_GPIO_NUM 34
#define VSYNC_GPIO_NUM 5
#define HREF_GPIO_NUM 27
#define PCLK_GPIO_NUM 25
#define I2C_SDA 21
#define I2C_SCL 22

// --- CONFIGURATION SETTINGS (Loaded from NVS) ---
extern int deep_sleep_seconds;
extern float storage_threshold_percent;

// --- GLOBAL OBJECTS ---
extern SSD1306 display;
extern Preferences preferences;
extern BLECharacteristic *pStatusCharacteristic;

// --- GLOBAL STATE FLAGS ---
extern volatile bool client_connected;
extern volatile bool next_chunk_requested;
extern volatile bool transfer_acknowledged;
extern volatile bool new_config_received;
extern volatile bool server_ready_for_data; // FIX: Flag for handshake
extern int image_count;
extern char pending_config_str[64]; // FIX: Buffer to hold incoming settings

// --- FUNCTION PROTOTYPES ---
void init_display();
void update_display(int line, const char *text, bool do_display);
void init_camera();
void deinit_camera();
void start_bluetooth();
void stop_bluetooth(); // New function for BLE shutdown
void send_batched_data();
bool store_image_in_psram();
void load_settings();
void apply_new_settings(); // FIX: New function to safely apply settings
void clear_image_buffers();

#endif // GLOBALS_H