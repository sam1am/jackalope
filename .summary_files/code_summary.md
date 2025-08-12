Project Root: /Users/johngarfield/Documents/GitHub/jackalope
Project Structure:
```
.
|-- .gitignore
|-- LICENSE
|-- README
|-- docs
    |-- pinout.md
|-- include
    |-- README
    |-- bluetooth_handler.h
    |-- camera_handler.h
    |-- display_handler.h
    |-- globals.h
|-- lib
    |-- README
|-- platformio.ini
|-- src
    |-- bluetooth_handler.cpp
    |-- camera_handler.cpp
    |-- display_handler.cpp
    |-- main.cpp
|-- srv
    |-- .gitignore
    |-- captures.db
    |-- imgs
        |-- 2025-08-12_12-00-32-052249.jpg
        |-- 2025-08-12_12-00-35-678699.jpg
        |-- 2025-08-12_12-00-39-668785.jpg
        |-- 2025-08-12_12-00-43-687061.jpg
        |-- 2025-08-12_12-00-48-306592.jpg
        |-- 2025-08-12_12-00-52-478963.jpg
        |-- 2025-08-12_12-00-56-767797.jpg
        |-- 2025-08-12_12-01-01-302113.jpg
        |-- 2025-08-12_12-01-05-140303.jpg
        |-- 2025-08-12_12-01-09-041226.jpg
        |-- 2025-08-12_12-01-12-997856.jpg
        |-- 2025-08-12_12-01-17-498697.jpg
        |-- 2025-08-12_12-01-21-642105.jpg
        |-- 2025-08-12_12-01-25-783130.jpg
        |-- 2025-08-12_12-02-36-045857.jpg
        |-- 2025-08-12_12-02-39-944720.jpg
        |-- 2025-08-12_12-02-44-294736.jpg
        |-- 2025-08-12_12-02-48-736106.jpg
        |-- 2025-08-12_12-02-53-387956.jpg
        |-- 2025-08-12_12-02-57-944395.jpg
        |-- 2025-08-12_12-03-02-388142.jpg
        |-- 2025-08-12_12-03-06-735758.jpg
        |-- 2025-08-12_12-03-11-174765.jpg
        |-- 2025-08-12_12-03-15-318974.jpg
        |-- 2025-08-12_12-03-20-144226.jpg
        |-- 2025-08-12_12-03-24-225321.jpg
        |-- 2025-08-12_12-03-28-395708.jpg
        |-- 2025-08-12_12-06-07-097225.jpg
        |-- 2025-08-12_12-06-10-846824.jpg
        |-- 2025-08-12_12-06-15-315699.jpg
        |-- 2025-08-12_12-06-19-396110.jpg
        |-- 2025-08-12_12-06-44-503344.jpg
        |-- 2025-08-12_12-06-49-156859.jpg
        |-- 2025-08-12_12-06-53-714563.jpg
        |-- 2025-08-12_12-06-58-250799.jpg
    |-- main.py
    |-- reqs.txt
    |-- static
        |-- latest.jpg
    |-- templates
        |-- index.html
|-- test
    |-- README

```

---
## File: include/globals.h

```h
#ifndef GLOBALS_H
#define GLOBALS_H

#include <Arduino.h>
#include "SSD1306.h"
#include "esp_camera.h"
#include <BLEDevice.h>
#include <Preferences.h>

// --- PROTOCOL & BATCH CONFIGURATION ---
#define CHUNK_SIZE 512      // The missing definition
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
extern int image_count;

// --- FUNCTION PROTOTYPES ---
void init_display();
void update_display(int line, const char *text, bool do_display);
void init_camera();
void deinit_camera();
void start_bluetooth();
void send_batched_data();
bool store_image_in_psram();
void load_settings();
void clear_image_buffers();

#endif // GLOBALS_H
```
---
## File: platformio.ini

```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino

; Serial Monitor options
monitor_speed = 115200

; --- SOLUTIONS IMPLEMENTED HERE ---

; 1. Add library dependency for the OLED display. The ESP32 core provides BLE.
lib_deps =
    thingpulse/ESP8266 and ESP32 OLED driver for SSD1306 displays

; 2. Enable PSRAM, tell the camera library which board we are using, and define the BLE device name.
; FIX: Corrected the escaping for the device name
build_flags =
    -D BOARD_HAS_PSRAM
    -D CAMERA_MODEL_TTGO_T_CAMERA_V162
    -D BLE_DEVICE_NAME=\"T-Camera-BLE-Batch\"

; 3. Ensure we have enough space for the large camera application firmware.
board_build.partitions = huge_app.csv
```
---
## File: src/bluetooth_handler.cpp

```cpp
#include "globals.h"
#include "display_handler.h"
#include <BLE2902.h>
#include <cstring> // Required for strcpy and strtok

// --- Image Buffers ---
// These are standard global variables, NOT in RTC memory.
// They are owned by this file and persist as long as the device is powered on.
uint8_t *framebuffers[IMAGE_BATCH_SIZE];
size_t fb_lengths[IMAGE_BATCH_SIZE];
int image_count = 0; // Tracks images in the current batch

// BLE Characteristics
BLECharacteristic *pStatusCharacteristic = NULL;
BLECharacteristic *pDataCharacteristic = NULL;
BLECharacteristic *pCommandCharacteristic = NULL;
BLECharacteristic *pConfigCharacteristic = NULL;

// --- Callback for handling settings changes from the server ---
class ConfigCallbacks : public BLECharacteristicCallbacks
{
    void onWrite(BLECharacteristic *pCharacteristic)
    {
        std::string value = pCharacteristic->getValue();
        if (value.length() > 0)
        {
            Serial.printf("Received new config string: %s\n", value.c_str());

            // Create a mutable buffer for strtok
            char buffer[value.length() + 1];
            strcpy(buffer, value.c_str());

            // Tokenize the string by the comma delimiter
            char *token = strtok(buffer, ",");
            while (token != NULL)
            {
                // Check the first character to identify the setting
                if (token[0] == 'F')
                {
                    int new_freq = atoi(token + 2); // Skip "F:"
                    if (new_freq > 0)
                    {
                        deep_sleep_seconds = new_freq;
                        Serial.printf("Parsed Frequency: %d\n", new_freq);
                    }
                }
                else if (token[0] == 'T')
                {
                    float new_thresh = atof(token + 2); // Skip "T:"
                    if (new_thresh >= 10 && new_thresh <= 95)
                    {
                        storage_threshold_percent = new_thresh;
                        Serial.printf("Parsed Threshold: %.1f\n", new_thresh);
                    }
                }
                token = strtok(NULL, ",");
            }

            // Save the updated values to non-volatile storage for persistence across reboots
            preferences.begin("settings", false);
            preferences.putInt("sleep_sec", deep_sleep_seconds);
            preferences.putFloat("storage_pct", storage_threshold_percent);
            preferences.end();

            Serial.printf("Settings saved and applied: Interval=%ds, Threshold=%.1f%%\n", deep_sleep_seconds, storage_threshold_percent);
            update_display(4, "Settings Saved!", true);
            delay(1500);
        }
    }
};

// --- Flow Control Callbacks ---
class CommandCallbacks : public BLECharacteristicCallbacks
{
    void onWrite(BLECharacteristic *pCharacteristic)
    {
        std::string value = pCharacteristic->getValue();
        if (value.length() > 0)
        {
            char cmd = value[0];
            if (cmd != 'N')
            {
                Serial.printf("Received command: %c (0x%02X)\n", cmd, cmd);
            }
            if (cmd == 'N')
            {
                next_chunk_requested = true;
            }
            else if (cmd == 'A')
            {
                transfer_acknowledged = true;
                Serial.println("ACK received from server");
            }
        }
    }
};

class MyServerCallbacks : public BLEServerCallbacks
{
    void onConnect(BLEServer *pServer)
    {
        client_connected = true;
        update_display(2, "Status: Connected");
        Serial.println("Client Connected.");
    }

    void onDisconnect(BLEServer *pServer)
    {
        client_connected = false;
        update_display(2, "Status: Disconnected");
        Serial.println("Client Disconnected.");
    }
};

void start_bluetooth()
{
    Serial.println("Starting BLE server...");
    BLEDevice::init(BLE_DEVICE_NAME);
    BLEDevice::setMTU(517);

    BLEServer *pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    BLEService *pService = pServer->createService(SERVICE_UUID);

    pStatusCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_STATUS,
        BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_READ);
    pStatusCharacteristic->addDescriptor(new BLE2902());

    pDataCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_DATA,
        BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_READ);
    pDataCharacteristic->addDescriptor(new BLE2902());

    pCommandCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_COMMAND,
        BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);
    pCommandCharacteristic->setCallbacks(new CommandCallbacks());

    pConfigCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_CONFIG,
        BLECharacteristic::PROPERTY_WRITE);
    pConfigCharacteristic->setCallbacks(new ConfigCallbacks());

    pService->start();

    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    BLEDevice::startAdvertising();

    Serial.println("Bluetooth Initialized and Advertising.");
    update_display(3, "BLE Init OK");
}

void notify_chunk(const uint8_t *data, size_t size)
{
    pDataCharacteristic->setValue((uint8_t *)data, size);
    pDataCharacteristic->notify();
    delay(10);
}

bool wait_for_acknowledgment(uint32_t timeout_ms)
{
    uint32_t start = millis();
    while (!transfer_acknowledged && millis() - start < timeout_ms)
    {
        if (!client_connected)
            return false;
        delay(10);
    }
    return transfer_acknowledged;
}

bool wait_for_next_chunk_request(uint32_t timeout_ms)
{
    uint32_t start = millis();
    next_chunk_requested = false;
    while (!next_chunk_requested && millis() - start < timeout_ms)
    {
        if (!client_connected)
            return false;
        delay(10);
    }
    return next_chunk_requested;
}

bool send_single_file_with_flow_control(const uint8_t *buffer, size_t total_size, const char *data_type, int image_num)
{
    if (total_size == 0)
        return true;

    char status_buf[32];
    sprintf(status_buf, "%s:%u", data_type, total_size);

    transfer_acknowledged = false;
    pStatusCharacteristic->setValue(status_buf);
    pStatusCharacteristic->notify();
    Serial.printf("[Image %d] Sent STATUS: %s. Waiting for ACK...\n", image_num, status_buf);
    delay(50);

    if (!wait_for_acknowledgment(10000))
    {
        Serial.printf("[Image %d] ERROR: Timeout waiting for server ACK.\n", image_num);
        return false;
    }

    Serial.printf("[Image %d] Server ACK received. Starting transfer (%u bytes)...\n", image_num, total_size);

    size_t sent = 0;
    int chunk_count = 0;

    size_t chunk_size = (total_size < CHUNK_SIZE) ? total_size : CHUNK_SIZE;
    notify_chunk(buffer, chunk_size);
    sent += chunk_size;
    chunk_count++;
    Serial.printf("[Image %d] Progress: %u/%u bytes\n", image_num, sent, total_size);

    while (sent < total_size)
    {
        if (!wait_for_next_chunk_request(15000))
        {
            Serial.printf("[Image %d] ERROR: Timeout waiting for chunk request at byte %u/%u\n", image_num, sent, total_size);
            return false;
        }

        size_t remaining = total_size - sent;
        chunk_size = (remaining < CHUNK_SIZE) ? remaining : CHUNK_SIZE;
        notify_chunk(buffer + sent, chunk_size);
        sent += chunk_size;
        chunk_count++;

        if (chunk_count % 5 == 0 || sent == total_size)
        {
            Serial.printf("[Image %d] Progress: %u/%u bytes\n", image_num, sent, total_size);
        }
    }

    Serial.printf("[Image %d] Transfer complete (%u bytes sent in %d chunks).\n", image_num, sent, chunk_count);
    delay(100);
    return true;
}

void send_batched_data()
{
    if (!client_connected)
        return;
    delay(500);

    char count_buf[32];
    sprintf(count_buf, "COUNT:%d", image_count);

    transfer_acknowledged = false;
    pStatusCharacteristic->setValue(count_buf);
    pStatusCharacteristic->notify();
    Serial.printf("Notified server: %s. Waiting for ACK...\n", count_buf);
    delay(50);

    if (!wait_for_acknowledgment(10000))
    {
        Serial.println("ERROR: Server did not acknowledge batch start. Aborting.");
        return;
    }

    Serial.println("Server acknowledged batch start. Beginning transfers...");
    for (int i = 0; i < image_count; i++)
    {
        if (!client_connected)
        {
            Serial.println("Client disconnected mid-batch. Aborting.");
            break;
        }
        Serial.printf("\n=== Sending image %d of %d (size: %u bytes) ===\n", i + 1, image_count, fb_lengths[i]);
        if (framebuffers[i] == NULL)
            continue;
        if (!send_single_file_with_flow_control(framebuffers[i], fb_lengths[i], "IMAGE", i + 1))
        {
            Serial.printf("Failed to send image %d. Aborting batch.\n", i + 1);
            break;
        }
        delay(500);
    }

    Serial.println("\n=== Batch Transfer Complete ===");
    // FIX: Removed the call to clear_image_buffers() from here.
    // The main loop is now responsible for memory management.
}
```
---
## File: src/main.cpp

```cpp
#include "globals.h"
#include "display_handler.h"
#include "esp_heap_caps.h"

// --- GLOBAL OBJECTS & VARIABLE DEFINITIONS ---
SSD1306 display(0x3c, I2C_SDA, I2C_SCL, GEOMETRY_128_64);
Preferences preferences;

// Configuration settings with defaults
int deep_sleep_seconds = 5;
float storage_threshold_percent = 5.0;

// Global state flags
volatile bool client_connected = false;
volatile bool next_chunk_requested = false;
volatile bool transfer_acknowledged = false;

// Forward declaration from bluetooth_handler.cpp, where these are defined
extern uint8_t *framebuffers[IMAGE_BATCH_SIZE];
extern size_t fb_lengths[IMAGE_BATCH_SIZE];
extern int image_count;

void clear_image_buffers()
{
  Serial.println("Clearing image buffers and freeing PSRAM...");
  for (int i = 0; i < IMAGE_BATCH_SIZE; i++)
  {
    if (framebuffers[i] != NULL)
    {
      heap_caps_free(framebuffers[i]);
      framebuffers[i] = NULL;
    }
    fb_lengths[i] = 0;
  }
  image_count = 0;
  Serial.println("Buffers cleared.");
}

bool store_image_in_psram()
{
  if (image_count >= IMAGE_BATCH_SIZE)
  {
    Serial.println("Image batch limit reached. Cannot store more images.");
    return false;
  }
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb)
  {
    Serial.println("Camera capture failed");
    return false;
  }
  framebuffers[image_count] = (uint8_t *)heap_caps_malloc(fb->len, MALLOC_CAP_SPIRAM);
  if (!framebuffers[image_count])
  {
    Serial.println("PSRAM malloc failed");
    esp_camera_fb_return(fb);
    return false;
  }
  memcpy(framebuffers[image_count], fb->buf, fb->len);
  fb_lengths[image_count] = fb->len;
  Serial.printf("Stored image %d in batch (%u bytes).\n", image_count + 1, fb_lengths[image_count]);
  image_count++;
  esp_camera_fb_return(fb);
  return true;
}

void load_settings()
{
  preferences.begin("settings", true);
  deep_sleep_seconds = preferences.getInt("sleep_sec", deep_sleep_seconds);
  storage_threshold_percent = preferences.getFloat("storage_pct", storage_threshold_percent);
  preferences.end();
  Serial.printf("Loaded Settings: Capture Interval = %d sec, Storage Threshold = %.1f%%\n",
                deep_sleep_seconds, storage_threshold_percent);
}

// --- SETUP: Runs once at power-on ---
void setup()
{
  Serial.begin(115200);
  Serial.println("\n--- T-Camera Continuous Timelapse Mode ---");

  init_display();
  load_settings();
  init_camera();
  start_bluetooth();

  update_display(0, "System Ready", true);
  Serial.println("System initialized and running. Waiting for first capture interval.");
}

// --- LOOP: Main program cycle ---
void loop()
{
  // 1. Wait for the specified interval. This uses the global variable that can be changed on-the-fly.
  Serial.printf("Waiting for %d seconds...\n", deep_sleep_seconds);
  delay(deep_sleep_seconds * 1000);

  // 2. Capture an image.
  Serial.println("Capture interval elapsed. Taking picture...");
  if (!store_image_in_psram())
  {
    Serial.println("Failed to store image. Check PSRAM or batch size limit.");
  }

  // 3. Check PSRAM usage and update status
  size_t total_psram = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
  size_t free_psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
  float used_percentage = total_psram > 0 ? (1.0 - ((float)free_psram / total_psram)) * 100.0 : 0;

  char status_buf[40];
  sprintf(status_buf, "PSRAM: %.1f%% | Imgs: %d", used_percentage, image_count);
  Serial.println(status_buf);
  update_display(1, status_buf, true);

  // Also notify the server of the current status if it's connected
  if (client_connected && pStatusCharacteristic != NULL)
  {
    pStatusCharacteristic->setValue(status_buf);
    pStatusCharacteristic->notify();
  }

  // 4. Decide whether to transfer the batch
  bool should_transfer = (image_count > 0 && (used_percentage >= storage_threshold_percent || image_count >= IMAGE_BATCH_SIZE));

  if (should_transfer)
  {
    Serial.printf("Transfer condition met (Usage: %.1f%%, Count: %d).\n", used_percentage, image_count);

    // FIX: Refactored logic to ensure clear_image_buffers() is always called once after any transfer attempt.
    bool transfer_attempted = false;

    if (client_connected)
    {
      Serial.println("Client is already connected. Starting transfer.");
      update_display(2, "Connected! Sending...", true);
      send_batched_data();
      transfer_attempted = true;
    }
    else
    {
      Serial.println("Waiting for a client to connect for transfer...");
      update_display(2, "Batch full. Wait conn.", true);

      uint32_t start_time = millis();
      while (!client_connected && (millis() - start_time < 30000))
      { // 30 second timeout
        delay(100);
      }

      if (client_connected)
      {
        Serial.println("Client connected for transfer.");
        update_display(2, "Connected! Sending...", true);
        send_batched_data();
      }
      else
      {
        Serial.println("No client connected within timeout. Discarding data to continue.");
        update_display(2, "No connection. Clearing.", true);
      }
      transfer_attempted = true;
    }

    if (transfer_attempted)
    {
      clear_image_buffers();
    }

    update_display(2, "", true); // Clear the status line after the attempt
  }
}
```
---
## File: srv/main.py

```py
import asyncio
import datetime
import os
import threading
import functools
import sqlite3
import time
from flask import Flask, render_template, send_from_directory, jsonify, request
from bleak import BleakScanner, BleakClient

# --- CONFIGURATION ---
FLASK_PORT = 5550
DEVICE_NAMES = ["T-Camera-BLE-Batch", "T-Camera-BLE"]
ROOT_DIR = os.path.dirname(os.path.abspath(__file__))
IMGS_FOLDER_NAME = 'imgs'
IMGS_PATH = os.path.join(ROOT_DIR, IMGS_FOLDER_NAME)
DB_PATH = os.path.join(ROOT_DIR, 'captures.db')

# --- BLE UUIDs ---
SERVICE_UUID = "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
CHARACTERISTIC_UUID_STATUS = "beb5483e-36e1-4688-b7f5-ea07361b26a8"
CHARACTERISTIC_UUID_DATA = "7347e350-5552-4822-8243-b8923a4114d2"
CHARACTERISTIC_UUID_COMMAND = "a244c201-1fb5-459e-8fcc-c5c9c331914b"
CHARACTERISTIC_UUID_CONFIG = "a31a6820-8437-4f55-8898-5226c04a29a3"

# --- PROTOCOL COMMANDS ---
CMD_NEXT_CHUNK = b'N'
CMD_ACKNOWLEDGE = b'A'

# --- WEB SERVER & GLOBAL STATE ---
app = Flask(__name__, static_folder=IMGS_FOLDER_NAME)
server_state = {
    "status": "Initializing...",
    "storage_usage": 0,
    "settings": {"frequency": 30, "threshold": 80}
}
data_queue = None
device_found_event = None
found_device = None
pending_config_command = None


# --- DATABASE HELPERS ---
def get_db_connection():
    conn = sqlite3.connect(DB_PATH)
    cursor = conn.cursor()
    cursor.execute('''
        CREATE TABLE IF NOT EXISTS captures (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp TEXT NOT NULL,
            image_path TEXT NOT NULL,
            gps_lat REAL,
            gps_lon REAL
        )
    ''')
    conn.commit()
    return conn


def setup_filesystem():
    if not os.path.exists(IMGS_PATH):
        print(f"Image directory not found. Creating at: {IMGS_PATH}")
        os.makedirs(IMGS_PATH)
    conn = get_db_connection()
    conn.close()
    print("Filesystem and database are ready.")


def db_insert_capture(timestamp, image_path):
    try:
        conn = get_db_connection()
        cursor = conn.cursor()
        cursor.execute(
            "INSERT INTO captures (timestamp, image_path) VALUES (?, ?)", (timestamp, image_path))
        conn.commit()
        conn.close()
    except sqlite3.Error as e:
        print(f"Database insert error: {e}")


# --- BLE LOGIC ---
async def transfer_file_data(client, expected_size, buffer, data_type):
    if expected_size == 0:
        return True
    bytes_received = 0
    try:
        print(f"Waiting for first chunk of {data_type}...")
        first_chunk = await asyncio.wait_for(data_queue.get(), timeout=15.0)
        buffer.extend(first_chunk)
        bytes_received = len(buffer)
        data_queue.task_done()
        print(
            f"Receiving {data_type}: {bytes_received}/{expected_size} bytes", end='\r')
        while bytes_received < expected_size:
            await client.write_gatt_char(CHARACTERISTIC_UUID_COMMAND, CMD_NEXT_CHUNK, response=False)
            chunk = await asyncio.wait_for(data_queue.get(), timeout=15.0)
            buffer.extend(chunk)
            bytes_received = len(buffer)
            data_queue.task_done()
            print(
                f"Receiving {data_type}: {bytes_received}/{expected_size} bytes", end='\r')
    except asyncio.TimeoutError:
        print(
            f"\nERROR: Timeout waiting for {data_type} data at {bytes_received}/{expected_size} bytes.")
        return False
    print(
        f"\n-> {data_type} transfer complete ({bytes_received} bytes received).")
    return True


def data_notification_handler(sender, data):
    if data_queue:
        data_queue.put_nowait(data)


async def handle_image_transfer(client, img_size):
    """
    Handles the image transfer in a separate, robust task to avoid blocking the main event handler.
    """
    try:
        server_state["status"] = f"Receiving image ({img_size} bytes)..."
        # Acknowledge the image transfer start
        await client.write_gatt_char(CHARACTERISTIC_UUID_COMMAND, CMD_ACKNOWLEDGE, response=False)
        img_buffer = bytearray()
        if await transfer_file_data(client, img_size, img_buffer, "Image"):
            timestamp = datetime.datetime.now()
            filename = timestamp.strftime("%Y-%m-%d_%H-%M-%S-%f") + ".jpg"
            filepath = os.path.join(IMGS_PATH, filename)
            with open(filepath, "wb") as f:
                f.write(img_buffer)
            db_insert_capture(timestamp.isoformat(),
                              os.path.join(IMGS_FOLDER_NAME, filename))
            print(f"-> Saved image to {filepath}")
            server_state["status"] = f"Image saved: {filename}"
        else:
            server_state["status"] = "Image transfer failed"
    except Exception as e:
        print(f"\nError during image transfer task: {e}")
        server_state["status"] = "Image transfer failed due to connection error."


async def status_notification_handler(sender, data, client):
    """
    Handles incoming status notifications from the device.
    For long operations like image transfers, it spawns a new task.
    """
    try:
        status_str = data.decode('utf-8').strip()
        print(f"\n[STATUS] Received: {status_str}")

        if status_str.startswith("PSRAM:"):
            try:
                usage_val = float(status_str.split(':')[1].split('%')[0])
                server_state["storage_usage"] = round(usage_val, 1)
            except (ValueError, IndexError):
                pass
            server_state["status"] = "Device ready to transfer."

        elif status_str.startswith("COUNT:"):
            image_count = int(status_str.split(':')[1])
            server_state["status"] = f"Batch of {image_count} images incoming. Acknowledging."
            print(server_state["status"])
            await client.write_gatt_char(CHARACTERISTIC_UUID_COMMAND, CMD_ACKNOWLEDGE, response=False)

        elif status_str.startswith("IMAGE:"):
            img_size = int(status_str.split(':')[1])
            # FIX: Spawn the transfer as a background task so it doesn't block this handler
            asyncio.create_task(handle_image_transfer(client, img_size))

    except Exception as e:
        print(f"Error in status_notification_handler: {e}")


def detection_callback(device, advertising_data):
    global found_device, device_found_event
    if device.name and any(name in device.name for name in DEVICE_NAMES):
        print(f"[SCAN] Target device found: {device.address} ({device.name})")
        if device_found_event and not device_found_event.is_set():
            found_device = device
            device_found_event.set()


async def ble_communication_task():
    global found_device, data_queue, device_found_event, pending_config_command
    data_queue = asyncio.Queue()
    device_found_event = asyncio.Event()

    while True:
        server_state["status"] = "Scanning for camera device..."
        print(f"\n{server_state['status']}")
        scanner = BleakScanner(detection_callback=detection_callback)
        try:
            await scanner.start()
            await asyncio.wait_for(device_found_event.wait(), timeout=120.0)
            await scanner.stop()
        except asyncio.TimeoutError:
            await scanner.stop()
            continue
        except Exception as e:
            print(f"Error during scan: {e}")
            await scanner.stop()
            continue

        if found_device:
            server_state["status"] = f"Connecting to {found_device.address}..."
            try:
                async with BleakClient(found_device, timeout=20.0) as client:
                    if client.is_connected:
                        server_state["status"] = "Connected. Setting up notifications..."
                        while not data_queue.empty():
                            data_queue.get_nowait()

                        status_handler_with_client = functools.partial(
                            status_notification_handler, client=client)
                        await client.start_notify(CHARACTERISTIC_UUID_STATUS, status_handler_with_client)
                        await client.start_notify(CHARACTERISTIC_UUID_DATA, data_notification_handler)

                        server_state["status"] = "Ready. Waiting for device data..."
                        while client.is_connected:
                            # This loop now runs un-blocked, allowing settings to be sent anytime.
                            if pending_config_command:
                                print(
                                    f"Sending config command: {pending_config_command}")
                                try:
                                    await client.write_gatt_char(
                                        CHARACTERISTIC_UUID_CONFIG,
                                        bytearray(
                                            pending_config_command, 'utf-8'),
                                        response=False
                                    )
                                    pending_config_command = None  # Clear after sending
                                    server_state["status"] = "Settings sent to device."
                                except Exception as e:
                                    print(f"Failed to send config: {e}")
                            await asyncio.sleep(1)
                        print("Client disconnected.")
            except Exception as e:
                server_state["status"] = f"Connection Error: {e}"
            finally:
                server_state["status"] = "Disconnected. Resuming scan."
                device_found_event.clear()
                found_device = None
                await asyncio.sleep(2)


# --- Flask Web Server ---
@app.route('/')
def index(): return render_template('index.html')


@app.route('/api/status')
def api_status():
    return jsonify({"status": server_state.get("status"), "storage_usage": server_state.get("storage_usage")})


@app.route('/api/captures')
def api_captures():
    try:
        conn = get_db_connection()
        conn.row_factory = sqlite3.Row
        captures = conn.execute(
            "SELECT * FROM captures ORDER BY timestamp DESC").fetchall()
        conn.close()
        return jsonify([dict(row) for row in captures])
    except sqlite3.Error as e:
        print(f"Database select error: {e}")
        return jsonify({"error": str(e)}), 500


@app.route('/api/settings', methods=['GET'])
def get_settings(): return jsonify(server_state["settings"])


@app.route('/api/settings', methods=['POST'])
def set_settings():
    global pending_config_command
    data = request.get_json()
    if not data or 'frequency' not in data or 'threshold' not in data:
        return jsonify({"error": "Invalid data"}), 400

    freq = int(data['frequency'])
    thresh = int(data['threshold'])
    server_state["settings"]["frequency"] = freq
    server_state["settings"]["threshold"] = thresh
    pending_config_command = f"F:{freq},T:{thresh}"

    server_state["status"] = "Settings queued. Will send on next connection."
    return jsonify({"message": "Settings queued successfully"})


@app.route('/imgs/<path:filename>')
def serve_image(filename): return send_from_directory(IMGS_PATH, filename)


def run_flask_app():
    print(f"Starting Flask web server on http://0.0.0.0:{FLASK_PORT}")
    app.run(host='0.0.0.0', port=FLASK_PORT, debug=False)


if __name__ == '__main__':
    setup_filesystem()
    threading.Thread(target=run_flask_app, daemon=True).start()
    time.sleep(1)
    try:
        print("Starting BLE communication task...")
        asyncio.run(ble_communication_task())
    except KeyboardInterrupt:
        print("\nProgram stopped by user.")
    except Exception as e:
        print(f"Fatal error in BLE task: {e}")

```
---
## File: srv/templates/index.html

```html
<!DOCTYPE html>
<html lang="en">

<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>T-Camera BLE Dashboard</title>
    <style>
        body {
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
            background-color: #f0f2f5;
            color: #333;
            margin: 0;
            padding: 20px;
        }

        .container {
            max-width: 1200px;
            margin: auto;
            background: #fff;
            padding: 20px;
            border-radius: 8px;
            box-shadow: 0 2px 4px rgba(0, 0, 0, 0.1);
        }

        h1,
        h2 {
            color: #1c1e21;
            border-bottom: 1px solid #eee;
            padding-bottom: 10px;
        }

        #status-bar {
            padding: 10px 15px;
            background-color: #e7f3ff;
            border: 1px solid #cce5ff;
            color: #004085;
            border-radius: 4px;
            margin-bottom: 20px;
        }

        .settings-form {
            background-color: #f9f9f9;
            padding: 20px;
            border: 1px solid #eee;
            border-radius: 8px;
            margin-top: 20px;
            margin-bottom: 30px;
        }

        .form-grid {
            display: grid;
            grid-template-columns: 1fr 1fr 1fr;
            gap: 20px;
            align-items: end;
        }

        .form-group label {
            display: block;
            margin-bottom: 5px;
            font-weight: bold;
        }

        .form-group input {
            width: 100%;
            box-sizing: border-box;
            padding: 8px;
            border-radius: 4px;
            border: 1px solid #ddd;
        }

        .form-group button {
            background-color: #007bff;
            color: white;
            width: 100%;
            padding: 10px 15px;
            border: none;
            border-radius: 4px;
            cursor: pointer;
            font-size: 1em;
        }

        .form-group button:hover {
            background-color: #0056b3;
        }

        #gallery {
            display: grid;
            grid-template-columns: repeat(auto-fill, minmax(280px, 1fr));
            gap: 15px;
        }

        .card {
            border: 1px solid #ddd;
            border-radius: 8px;
            overflow: hidden;
            box-shadow: 0 1px 2px rgba(0, 0, 0, 0.05);
        }

        .card img {
            max-width: 100%;
            height: auto;
            display: block;
        }

        .card-info {
            padding: 10px;
            background-color: #fafafa;
            font-size: 0.9em;
        }

        .card-info p {
            margin: 0 0 5px 0;
        }
    </style>
</head>

<body>
    <div class="container">
        <h1>T-Camera BLE Dashboard</h1>
        <div id="status-bar">
            <strong>Status:</strong> <span id="status-text">Loading...</span> |
            <strong>Storage Used:</strong> <span id="storage-usage-text">0</span>%
        </div>

        <div class="settings-form">
            <h2>Device Settings</h2>
            <div class="form-grid">
                <div class="form-group">
                    <label for="capture-frequency">Capture Frequency (seconds)</label>
                    <input type="number" id="capture-frequency" min="5">
                </div>
                <div class="form-group">
                    <label for="storage-threshold">Send Threshold (%)</label>
                    <input type="number" id="storage-threshold" min="10" max="95">
                </div>
                <div class="form-group">
                    <button id="save-settings-btn">Save & Send to Device</button>
                </div>
            </div>
        </div>

        <h2>Captured Images</h2>
        <div id="gallery">
            <p>Waiting for data...</p>
        </div>
    </div>

    <script>
        const gallery = document.getElementById('gallery');
        const statusText = document.getElementById('status-text');
        const storageUsageText = document.getElementById('storage-usage-text');
        const freqInput = document.getElementById('capture-frequency');
        const threshInput = document.getElementById('storage-threshold');
        const saveBtn = document.getElementById('save-settings-btn');

        async function fetchStatus() {
            try {
                const response = await fetch('/api/status');
                const data = await response.json();
                statusText.textContent = data.status || 'N/A';
                storageUsageText.textContent = data.storage_usage || '0';
            } catch (error) {
                console.error('Error fetching status:', error);
                statusText.textContent = 'Error loading status.';
            }
        }

        async function fetchSettings() {
            try {
                const response = await fetch('/api/settings');
                const data = await response.json();
                freqInput.value = data.frequency;
                threshInput.value = data.threshold;
            } catch (error) {
                console.error('Error fetching settings:', error);
            }
        }

        async function saveSettings() {
            const settings = {
                frequency: parseInt(freqInput.value, 10),
                threshold: parseInt(threshInput.value, 10)
            };
            statusText.textContent = "Queueing settings for device...";
            try {
                const response = await fetch('/api/settings', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify(settings)
                });
                const result = await response.json();
                statusText.textContent = result.message || "Settings sent!";
            } catch (error) {
                console.error('Error saving settings:', error);
                statusText.textContent = "Error saving settings.";
            }
        }

        async function fetchCaptures() {
            try {
                const response = await fetch('/api/captures');
                const captures = await response.json();
                if (captures.length === 0) {
                    gallery.innerHTML = '<p>No images have been received yet.</p>';
                    return;
                }
                gallery.innerHTML = '';
                captures.forEach(capture => {
                    const card = document.createElement('div');
                    card.className = 'card';
                    const img = document.createElement('img');
                    img.src = capture.image_path;
                    img.alt = `Capture from ${capture.timestamp}`;
                    const info = document.createElement('div');
                    info.className = 'card-info';
                    const timestamp = new Date(capture.timestamp).toLocaleString();
                    info.innerHTML = `<p><strong>Timestamp:</strong> ${timestamp}</p>`;
                    card.appendChild(img);
                    card.appendChild(info);
                    gallery.appendChild(card);
                });
            } catch (error) {
                console.error('Error fetching captures:', error);
                gallery.innerHTML = '<p>Error loading images.</p>';
            }
        }

        saveBtn.addEventListener('click', saveSettings);

        setInterval(() => {
                fetchStatus();
                fetchCaptures();
            }, 3000);

            document.addEventListener('DOMContentLoaded', () => {
                fetchStatus();
                fetchCaptures();
                fetchSettings();
            });
        </script>
    </body>
    
</html>
```
---
