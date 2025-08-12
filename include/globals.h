#ifndef GLOBALS_H
#define GLOBALS_H

#include <Arduino.h>
#include "SSD1306.h"
#include "esp_camera.h"
#include <BLEDevice.h>

// --- PROTOCOL & PIN DEFINITIONS (Unchanged) ---
#define CHUNK_SIZE 512
#ifndef BLE_DEVICE_NAME
#define BLE_DEVICE_NAME "T-Camera-BLE"
#endif

// --- BLE UUIDs (Unchanged) ---
#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID_STATUS "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define CHARACTERISTIC_UUID_DATA "7347e350-5552-4822-8243-b8923a4114d2"
#define CHARACTERISTIC_UUID_COMMAND "a244c201-1fb5-459e-8fcc-c5c9c331914b"

// --- Pin & Audio Definitions (Unchanged) ---
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
#define BTN_GPIO_NUM 15
#define I2S_SCK 26
#define I2S_WS 32
#define I2S_SD 33
#define SAMPLE_RATE 8000
#define RECORD_DURATION 10
#define HEADER_SIZE 44

// --- CAPTURE MODE CONTROL ---
enum CaptureMode
{
    MODE_BOTH,
    MODE_IMAGE_ONLY,
    MODE_AUDIO_ONLY,
    MODE_COUNT
};
extern const char *mode_names[];
extern volatile CaptureMode current_mode;
extern volatile bool mode_changed;

// --- GLOBAL OBJECTS ---
extern SSD1306 display;
// extern hw_timer_t *timer; // REMOVED
extern BLECharacteristic *pStatusCharacteristic;
extern BLECharacteristic *pDataCharacteristic;

// --- GLOBAL STATE FLAGS ---
extern volatile bool client_connected;
extern volatile bool next_chunk_requested;
extern volatile bool transfer_complete;
extern volatile bool capture_requested; // The new server-driven trigger

// --- FUNCTION PROTOTYPES ---
void init_display();
void init_camera();
void init_audio();
void init_hardware();
void start_bluetooth();
void handle_capture_cycle();
void update_mode_display();

#endif // GLOBALS_H