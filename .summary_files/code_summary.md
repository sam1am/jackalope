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
        |-- 2025-08-12_10-34-30-465720.jpg
        |-- 2025-08-12_10-35-03-538132.jpg
        |-- 2025-08-12_10-35-35-761268.jpg
        |-- 2025-08-12_10-36-07-942298.jpg
        |-- 2025-08-12_10-36-40-100989.jpg
        |-- 2025-08-12_10-37-12-711480.jpg
        |-- 2025-08-12_10-37-45-204457.jpg
        |-- 2025-08-12_10-38-17-543799.jpg
        |-- 2025-08-12_10-38-50-002978.jpg
        |-- 2025-08-12_10-39-22-402469.jpg
        |-- 2025-08-12_10-53-58-902877.jpg
        |-- 2025-08-12_10-54-02-499688.jpg
        |-- 2025-08-12_10-54-37-807403.jpg
        |-- 2025-08-12_10-54-41-411827.jpg
        |-- 2025-08-12_10-55-16-630717.jpg
        |-- 2025-08-12_10-55-20-169216.jpg
        |-- 2025-08-12_10-55-55-361646.jpg
        |-- 2025-08-12_10-55-58-687618.jpg
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

// --- PROTOCOL & PIN DEFINITIONS ---
#define CHUNK_SIZE 512
#ifndef BLE_DEVICE_NAME
#define BLE_DEVICE_NAME "T-Camera-BLE-Batch"
#endif

// --- SLEEP & BATCH CONFIGURATION ---
#define DEEP_SLEEP_SECONDS 10 // Time between wake-ups
#define IMAGE_BATCH_SIZE 2    // Number of wake-ups before capturing and sending

// --- BLE UUIDs (Unchanged) ---
#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID_STATUS "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define CHARACTERISTIC_UUID_DATA "7347e350-5552-4822-8243-b8923a4114d2"
#define CHARACTERISTIC_UUID_COMMAND "a244c201-1fb5-459e-8fcc-c5c9c331914b"

// --- Pin Definitions ---
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

// --- DEEP SLEEP PERSISTENT STATE ---
// This counter tracks the number of times the device has woken up.
extern RTC_DATA_ATTR int wake_count;

// --- GLOBAL OBJECTS ---
extern SSD1306 display;
extern BLECharacteristic *pStatusCharacteristic;
extern BLECharacteristic *pDataCharacteristic;

// --- GLOBAL STATE FLAGS ---
extern volatile bool client_connected;
extern volatile bool next_chunk_requested;
extern volatile bool transfer_acknowledged;
// This counter is for the current batch, used only during an active connection.
extern int image_count;

// --- FUNCTION PROTOTYPES ---
void init_display();
void update_display(int line, const char *text, bool do_display);
void init_camera();
void deinit_camera();
void start_bluetooth();
void send_batched_data();
void enter_deep_sleep(bool camera_was_active);
bool store_image_in_psram();

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

// --- RTC (Deep Sleep) Memory ---
// These are no longer in RTC_DATA_ATTR as they are only used in one boot cycle
uint8_t *framebuffers[IMAGE_BATCH_SIZE];
size_t fb_lengths[IMAGE_BATCH_SIZE];
// Regular global variable for the number of images in the current batch
int image_count = 0;

// BLE Characteristics
BLECharacteristic *pStatusCharacteristic = NULL;
BLECharacteristic *pDataCharacteristic = NULL;
BLECharacteristic *pCommandCharacteristic = NULL;

// --- Flow Control Callbacks ---
class CommandCallbacks : public BLECharacteristicCallbacks
{
    void onWrite(BLECharacteristic *pCharacteristic)
    {
        std::string value = pCharacteristic->getValue();
        if (value.length() > 0)
        {
            char cmd = value[0];
            // Don't print for 'N' to reduce log spam
            if (cmd != 'N')
            {
                Serial.printf("Received command: %c (0x%02X)\n", cmd, cmd);
            }

            if (cmd == 'N') // "Next" chunk
            {
                next_chunk_requested = true;
            }
            else if (cmd == 'A') // "Acknowledged" transfer start
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
        Serial.println("Client Disconnected.");
    }
};

void start_bluetooth()
{
    Serial.println("Starting BLE server for batch transfer...");
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
    delay(10); // Small delay for notification to send
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

    // 1. Send status (size) of the current file
    char status_buf[32];
    sprintf(status_buf, "%s:%u", data_type, total_size);

    transfer_acknowledged = false;
    pStatusCharacteristic->setValue(status_buf);
    pStatusCharacteristic->notify();
    Serial.printf("[Image %d] Sent STATUS: %s. Waiting for ACK...\n", image_num, status_buf);

    delay(50);

    // 2. Wait for server to acknowledge it's ready for this file
    if (!wait_for_acknowledgment(10000))
    {
        Serial.printf("[Image %d] ERROR: Timeout waiting for server to acknowledge file transfer start.\n", image_num);
        return false;
    }

    Serial.printf("[Image %d] Server ACK received. Starting %s transfer (%u bytes)...\n", image_num, data_type, total_size);

    // 3. Send the file data in chunks
    size_t sent = 0;
    int chunk_count = 0;

    // --- MODIFIED LOGIC ---
    // The ACK serves as the request for the FIRST chunk. Send it immediately.
    size_t chunk_size = (total_size < CHUNK_SIZE) ? total_size : CHUNK_SIZE;
    notify_chunk(buffer, chunk_size);
    sent += chunk_size;
    chunk_count++;
    Serial.printf("[Image %d] Progress: %u/%u bytes (Sent first chunk on ACK)\n", image_num, sent, total_size);

    // Now, for all SUBSEQUENT chunks, wait for the 'N' command from the server.
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
    // --- END MODIFIED LOGIC ---

    Serial.printf("[Image %d] Transfer complete (%u bytes sent in %d chunks).\n", image_num, sent, chunk_count);

    delay(100);
    return true;
}

void send_batched_data()
{
    if (!client_connected)
    {
        Serial.println("ERROR: send_batched_data called but no client connected");
        return;
    }

    delay(500); // Give client time to be fully ready

    // 1. Notify the server how many images are in the batch
    char count_buf[32];
    sprintf(count_buf, "COUNT:%d", image_count);

    transfer_acknowledged = false;
    pStatusCharacteristic->setValue(count_buf);
    pStatusCharacteristic->notify();
    Serial.printf("Notified server: %s. Waiting for ACK...\n", count_buf);

    delay(50); // Ensure notification is sent

    // 2. Wait for server to acknowledge it's ready for the batch
    if (!wait_for_acknowledgment(10000))
    {
        Serial.println("ERROR: Server did not acknowledge batch start. Aborting.");
        return;
    }

    Serial.println("Server acknowledged batch start. Beginning transfers...");

    // 3. Loop through and send each stored image
    for (int i = 0; i < image_count; i++)
    {
        if (!client_connected)
        {
            Serial.println("Client disconnected mid-batch. Aborting.");
            break;
        }

        Serial.printf("\n=== Sending image %d of %d (size: %u bytes) ===\n", i + 1, image_count, fb_lengths[i]);

        if (framebuffers[i] == NULL)
        {
            Serial.printf("ERROR: Image %d buffer is NULL!\n", i);
            continue;
        }

        if (!send_single_file_with_flow_control(framebuffers[i], fb_lengths[i], "IMAGE", i + 1))
        {
            Serial.printf("Failed to send image %d. Aborting batch.\n", i + 1);
            break;
        }
        delay(500);
    }

    Serial.println("\n=== Batch Transfer Complete ===");

    for (int i = 0; i < image_count; i++)
    {
        if (framebuffers[i] != NULL)
        {
            heap_caps_free(framebuffers[i]);
            framebuffers[i] = NULL;
            fb_lengths[i] = 0;
        }
    }
    image_count = 0;
    wake_count = 0;

    Serial.println("Memory freed and all counters reset.");
}
```
---
## File: src/main.cpp

```cpp
#include "globals.h"
#include "display_handler.h"
#include "esp_heap_caps.h"

// --- RTC (Deep Sleep) Memory ---
RTC_DATA_ATTR int wake_count = 0;

// Define global objects
SSD1306 display(0x3c, I2C_SDA, I2C_SCL, GEOMETRY_128_64);

// Define global state flags
volatile bool client_connected = false;
volatile bool next_chunk_requested = false;
volatile bool transfer_acknowledged = false;

// Forward declaration from bluetooth_handler.cpp
extern uint8_t *framebuffers[IMAGE_BATCH_SIZE];
extern size_t fb_lengths[IMAGE_BATCH_SIZE];
extern int image_count; // Use the non-RTC version

bool store_image_in_psram()
{
  if (image_count >= IMAGE_BATCH_SIZE)
  {
    Serial.println("PSRAM buffer is full.");
    return false;
  }

  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb)
  {
    Serial.println("Camera capture failed");
    return false;
  }

  // Use heap_caps_malloc to allocate from PSRAM explicitly
  framebuffers[image_count] = (uint8_t *)heap_caps_malloc(fb->len, MALLOC_CAP_SPIRAM);
  if (!framebuffers[image_count])
  {
    Serial.println("PSRAM malloc failed");
    esp_camera_fb_return(fb);
    return false;
  }

  // Copy the image data
  memcpy(framebuffers[image_count], fb->buf, fb->len);
  fb_lengths[image_count] = fb->len;

  Serial.printf("Stored image %d in batch (%u bytes) in PSRAM at %p.\n",
                image_count + 1, fb_lengths[image_count], framebuffers[image_count]);

  esp_camera_fb_return(fb);
  image_count++;
  return true;
}

void enter_deep_sleep(bool camera_was_active)
{
  if (camera_was_active)
  {
    deinit_camera();
  }
  display.clear();
  char sleep_buf[32];
  sprintf(sleep_buf, "Sleeping... (%d/%d)", wake_count, IMAGE_BATCH_SIZE);
  display.drawString(0, 0, sleep_buf);
  display.display();
  Serial.printf("Entering deep sleep for %d seconds... (Wake count: %d)\n\n", DEEP_SLEEP_SECONDS, wake_count);
  esp_sleep_enable_timer_wakeup(DEEP_SLEEP_SECONDS * 1000000);
  esp_deep_sleep_start();
}

void setup()
{
  Serial.begin(115200);
  Serial.println("\n--- T-Camera BLE Batch Transfer ---");

  init_display();
  wake_count++; // Increment wake count on every wake-up

  char status_buf[32];
  sprintf(status_buf, "Wake count: %d/%d", wake_count, IMAGE_BATCH_SIZE);
  update_display(0, "Device Woke Up!", false);
  update_display(1, status_buf, true);

  // If the batch interval is not yet reached, just go back to sleep.
  if (wake_count < IMAGE_BATCH_SIZE)
  {
    Serial.printf("This is wake %d of %d. Going back to sleep.\n", wake_count, IMAGE_BATCH_SIZE);
    delay(1000); // Give time to read display
    enter_deep_sleep(false);
  }
  // If the batch interval is reached, capture all images now and then transmit.
  else
  {
    Serial.printf("Batch interval reached (%d wakes). Starting capture process.\n", wake_count);
    update_display(2, "Capturing batch...", true);

    init_camera();

    // Capture the entire batch of images in this one cycle
    for (int i = 0; i < IMAGE_BATCH_SIZE; i++)
    {
      char capture_status[32];
      sprintf(capture_status, "Capturing %d/%d...", i + 1, IMAGE_BATCH_SIZE);
      update_display(3, capture_status, true);

      if (!store_image_in_psram())
      {
        Serial.printf("Failed to capture image %d. Aborting.\n", i + 1);
        update_display(4, "Capture FAILED", true);
        delay(2000);
        // Reset wake count and sleep even on failure
        wake_count = 0;
        enter_deep_sleep(true);
        return; // Should not be reached
      }
      delay(500); // Small delay between captures
    }

    // All images are now stored in memory. Now start BLE.
    update_display(2, "Batch ready. Start BLE", true);
    start_bluetooth();
    Serial.println("Advertising started. Waiting for connection...");

    uint32_t start_time = millis();
    // Wait for a client to connect for up to 60 seconds
    while (!client_connected && (millis() - start_time < 60000))
    {
      delay(100);
    }

    if (client_connected)
    {
      Serial.println("Client connected. Giving client time to prepare...");
      delay(3000); // Give the client time to discover services and set up notifications

      Serial.println("Starting batch data transfer...");
      send_batched_data(); // This function now also resets wake_count on success

      // Wait a moment for client to disconnect or for final data to be processed
      uint32_t disconnect_wait_start = millis();
      while (client_connected && (millis() - disconnect_wait_start < 5000))
      {
        delay(100);
      }
    }
    else
    {
      Serial.println("No client connected within 60s timeout. Discarding data.");
      // If no one connects, we must reset the wake_count to start a new cycle.
      wake_count = 0;
    }

    // De-init camera and go to sleep. The wake_count is reset by send_batched_data() on success
    // or just above on timeout.
    enter_deep_sleep(true);
  }
}

void loop()
{
  // Intentionally empty. All logic is in setup() due to deep sleep.
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
from flask import Flask, render_template, send_from_directory, jsonify
from bleak import BleakScanner, BleakClient
from bleak.exc import BleakError
import time

# --- CONFIGURATION ---
FLASK_PORT = 5550
DEVICE_NAMES = ["T-Camera-BLE-Batch", "T-Camera-BLE"]
ROOT_DIR = os.path.dirname(os.path.abspath(__file__))
IMGS_FOLDER_NAME = 'imgs'
IMGS_PATH = os.path.join(ROOT_DIR, IMGS_FOLDER_NAME)
DB_PATH = os.path.join(ROOT_DIR, 'captures.db')

# --- BLE UUIDS ---
SERVICE_UUID = "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
CHARACTERISTIC_UUID_STATUS = "beb5483e-36e1-4688-b7f5-ea07361b26a8"
CHARACTERISTIC_UUID_DATA = "7347e350-5552-4822-8243-b8923a4114d2"
CHARACTERISTIC_UUID_COMMAND = "a244c201-1fb5-459e-8fcc-c5c9c331914b"

# --- PROTOCOL COMMANDS ---
CMD_NEXT_CHUNK = b'N'
CMD_ACKNOWLEDGE = b'A'

# --- WEB SERVER & GLOBAL STATE ---
app = Flask(__name__, static_folder=IMGS_FOLDER_NAME)
server_state = {"status": "Initializing..."}
data_queue = None
device_found_event = None
found_device = None

# --- DATABASE SETUP ---


def setup_database():
    if not os.path.exists(IMGS_PATH):
        os.makedirs(IMGS_PATH)
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
    conn.close()


def db_insert_capture(timestamp, image_path, gps_lat=None, gps_lon=None):
    conn = sqlite3.connect(DB_PATH)
    cursor = conn.cursor()
    cursor.execute(
        "INSERT INTO captures (timestamp, image_path, gps_lat, gps_lon) VALUES (?, ?, ?, ?)",
        (timestamp, image_path, gps_lat, gps_lon)
    )
    conn.commit()
    conn.close()

# --- BLE LOGIC ---


async def transfer_file_data(client, expected_size, buffer, data_type):
    if expected_size == 0:
        return True
    bytes_received = 0
    try:
        # --- MODIFIED LOGIC ---
        # 1. Wait for the FIRST chunk. The ESP32 sends this automatically after we ACK the file start.
        print(f"Waiting for first chunk of {data_type}...")
        first_chunk = await asyncio.wait_for(data_queue.get(), timeout=15.0)
        buffer.extend(first_chunk)
        bytes_received = len(buffer)
        data_queue.task_done()
        print(
            f"Receiving {data_type}: {bytes_received}/{expected_size} bytes", end='\r')

        # 2. Now loop for the rest of the data, requesting each subsequent chunk with 'N'.
        while bytes_received < expected_size:
            await client.write_gatt_char(CHARACTERISTIC_UUID_COMMAND, CMD_NEXT_CHUNK, response=False)
            chunk = await asyncio.wait_for(data_queue.get(), timeout=15.0)
            buffer.extend(chunk)
            bytes_received = len(buffer)
            data_queue.task_done()
            print(
                f"Receiving {data_type}: {bytes_received}/{expected_size} bytes", end='\r')
        # --- END MODIFIED LOGIC ---

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


async def status_notification_handler(sender, data, client):
    try:
        status_str = data.decode('utf-8').strip()
        print(f"\n[STATUS] Received: {status_str}")

        if status_str.startswith("COUNT:"):
            parts = status_str.split(':')
            image_count = int(parts[1])
            server_state["status"] = f"Batch of {image_count} images detected. Acknowledging."
            print(server_state["status"])
            await client.write_gatt_char(CHARACTERISTIC_UUID_COMMAND, CMD_ACKNOWLEDGE, response=False)
            print("Sent ACK for batch start")

        elif status_str.startswith("IMAGE:"):
            parts = status_str.split(':')
            img_size = int(parts[1])
            server_state["status"] = f"Receiving image ({img_size} bytes)..."
            print(f"Image transfer starting. Expecting {img_size} bytes.")
            await client.write_gatt_char(CHARACTERISTIC_UUID_COMMAND, CMD_ACKNOWLEDGE, response=False)
            print("Sent ACK for image start")

            img_buffer = bytearray()
            if await transfer_file_data(client, img_size, img_buffer, "Image"):
                timestamp = datetime.datetime.now()
                filename = timestamp.strftime("%Y-%m-%d_%H-%M-%S-%f") + ".jpg"
                filepath = os.path.join(IMGS_PATH, filename)
                with open(filepath, "wb") as f:
                    f.write(img_buffer)
                db_insert_capture(timestamp.isoformat(),
                                  os.path.join(IMGS_FOLDER_NAME, filename))
                print(f"-> Saved image to {filepath} and logged to DB.")
                server_state["status"] = f"Image saved: {filename}"
            else:
                print("Image transfer failed.")
                server_state["status"] = "Image transfer failed"
    except Exception as e:
        print(f"Error in status_notification_handler: {e}")
        import traceback
        traceback.print_exc()


def detection_callback(device, advertising_data):
    global found_device, device_found_event
    service_uuids = advertising_data.service_uuids or []

    is_target = False
    if device.name:
        for target_name in DEVICE_NAMES:
            if target_name in device.name:
                is_target = True
                break

    if not is_target and SERVICE_UUID.lower() in [s.lower() for s in service_uuids]:
        is_target = True

    if is_target:
        print(f"[SCAN] Target device found: {device.address} ({device.name})")
        if device_found_event and not device_found_event.is_set():
            found_device = device
            device_found_event.set()


async def ble_communication_task():
    global found_device, data_queue, device_found_event
    data_queue = asyncio.Queue()
    device_found_event = asyncio.Event()

    while True:
        server_state["status"] = f"Scanning for camera device..."
        print(f"\n{server_state['status']}")

        scanner = BleakScanner(detection_callback=detection_callback)

        try:
            await scanner.start()
            await asyncio.wait_for(device_found_event.wait(), timeout=120.0)
            await scanner.stop()
        except asyncio.TimeoutError:
            print(f"No device found after 120 seconds. Restarting scan...")
            await scanner.stop()
            await asyncio.sleep(2)
            continue
        except Exception as e:
            print(f"Error during scan: {e}")
            await scanner.stop()
            await asyncio.sleep(5)
            continue

        if found_device:
            server_state["status"] = f"Connecting to {found_device.address}..."
            print(server_state["status"])
            try:
                async with BleakClient(found_device, timeout=20.0) as client:
                    if client.is_connected:
                        server_state["status"] = "Connected. Setting up notifications..."
                        print(server_state["status"])

                        while not data_queue.empty():
                            data_queue.get_nowait()

                        status_handler_with_client = functools.partial(
                            status_notification_handler, client=client)

                        await client.start_notify(CHARACTERISTIC_UUID_STATUS, status_handler_with_client)
                        print("STATUS notifications enabled")
                        await client.start_notify(CHARACTERISTIC_UUID_DATA, data_notification_handler)
                        print("DATA notifications enabled")

                        server_state["status"] = "Ready. Waiting for device to send data..."
                        print(server_state["status"])

                        while client.is_connected:
                            await asyncio.sleep(1)
                        print("Client disconnected")

            except Exception as e:
                server_state["status"] = f"Connection Error: {e}"
                print(f"An unexpected error occurred: {e}")
            finally:
                server_state["status"] = "Disconnected. Resuming scan."
                print(server_state["status"])
                device_found_event.clear()
                found_device = None
                await asyncio.sleep(2)
        else:
            device_found_event.clear()
            await asyncio.sleep(2)


# --- Flask Web Server ---
@app.route('/')
def index():
    return render_template('index.html')


@app.route('/api/status')
def api_status():
    return jsonify(server_state)


@app.route('/api/captures')
def api_captures():
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row
    cursor = conn.cursor()
    captures = cursor.execute(
        "SELECT * FROM captures ORDER BY timestamp DESC").fetchall()
    conn.close()
    return jsonify([dict(row) for row in captures])


@app.route('/imgs/<path:filename>')
def serve_image(filename):
    return send_from_directory(IMGS_PATH, filename)


def run_flask_app():
    print(f"Starting Flask web server on http://0.0.0.0:{FLASK_PORT}")
    app.run(host='0.0.0.0', port=FLASK_PORT, debug=False)


if __name__ == '__main__':
    setup_database()
    flask_thread = threading.Thread(target=run_flask_app, daemon=True)
    flask_thread.start()
    time.sleep(1)

    try:
        print("Starting BLE communication task...")
        asyncio.run(ble_communication_task())
    except KeyboardInterrupt:
        print("\nProgram stopped by user.")
    except Exception as e:
        print(f"Fatal error in BLE task: {e}")
        import traceback
        traceback.print_exc()

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
        }

        #status-bar {
            padding: 10px 15px;
            background-color: #e7f3ff;
            border: 1px solid #cce5ff;
            color: #004085;
            border-radius: 4px;
            margin-bottom: 20px;
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
            <strong>Status:</strong> <span id="status-text">Loading...</span>
        </div>

        <h2>Captured Images</h2>
        <div id="gallery">
            <p>No images loaded yet. Waiting for data...</p>
        </div>
    </div>

    <script>
        const gallery = document.getElementById('gallery');
        const statusText = document.getElementById('status-text');

        async function fetchStatus() {
            try {
                const response = await fetch('/api/status');
                const data = await response.json();
                statusText.textContent = data.status || 'N/A';
            } catch (error) {
                console.error('Error fetching status:', error);
                statusText.textContent = 'Error loading status.';
            }
        }

        async function fetchCaptures() {
            try {
                const response = await fetch('/api/captures');
                const captures = await response.json();

                if (captures.length === 0) {
                    gallery.innerHTML = '<p>No images have been captured yet.</p>';
                    return;
                }

                gallery.innerHTML = ''; // Clear previous content
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

                    if (capture.gps_lat && capture.gps_lon) {
                        info.innerHTML += `<p><strong>GPS:</strong> ${capture.gps_lat}, ${capture.gps_lon}</p>`;
                    }

                    card.appendChild(img);
                    card.appendChild(info);
                    gallery.appendChild(card);
                });
            } catch (error) {
                console.error('Error fetching captures:', error);
                gallery.innerHTML = '<p>Error loading images.</p>';
            }
        }

        // Fetch data every 3 seconds
        setInterval(() => {
            fetchStatus();
            fetchCaptures();
        }, 3000);

        // Initial load
        document.addEventListener('DOMContentLoaded', () => {
            fetchStatus();
            fetchCaptures();
        });
    </script>
</body>

</html>
```
---
