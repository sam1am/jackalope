Project Root: /Users/johngarfield/Documents/PlatformIO/Projects/SamTest
Project Structure:
```
.
|-- .gitignore
|-- docs
    |-- pinout.md
|-- include
    |-- README
    |-- audio_handler.h
    |-- bluetooth_handler.h
    |-- camera_handler.h
    |-- display_handler.h
    |-- globals.h
    |-- hardware_handler.h
|-- lib
    |-- README
|-- platformio.ini
|-- src
    |-- audio_handler.cpp
    |-- bluetooth_handler.cpp
    |-- camera_handler.cpp
    |-- display_handler.cpp
    |-- hardware_handler.cpp
    |-- main.cpp
|-- srv
    |-- .gitignore
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
## File: docs/pinout.md

```md

<h1 align = "center">🌟 T Camear V162 🌟</h1>


|    Name    | T-Camear V162 |
| :--------: | :-----------: |
|   DVP Y9   |      36       |
|   DVP Y8   |      37       |
|   DVP Y7   |      38       |
|   DVP Y6   |      39       |
|   DVP Y5   |      35       |
|   DVP Y4   |      14       |
|   DVP Y3   |      13       |
|   DVP Y2   |      34       |
|  DVP VSNC  |       5       |
|  DVP HREF  |      27       |
|  DVP PCLK  |      25       |
|  DVP PWD   |      N/A      |
|  DVP XCLK  |       4       |
|  DVP SIOD  |      18       |
|  DVP SIOC  |      23       |
| DVP RESET  |      N/A      |
|    SDA     |      21       |
|    SCL     |      22       |
|   Button   |      15       |
|    PIR     |      19       |
| OLED Model |    SSD1306    |
| TFT Width  |      128      |
| TFT Height |      64       |
|  IIS SCK   |      26       |
|   IIS WS   |      32       |
|  IIS DOUT  |      33       |

* Note: **PIR** Pin not **RTC IO**, unable to wake from deep sleep

### Programming Notes:
1. When using **T-Camear V162** ,uncomment **CAMERA_MODEL_TTGO_T_CAMERA_V162** in **sketch.ino**
1. The following libraries need to be installed to compile
    - [mathertel/OneButton](https://github.com/mathertel/OneButton) 
    - [ThingPulse/esp8266-oled-ssd1306](https://github.com/ThingPulse/esp8266-oled-ssd1306)

```
---
## File: include/audio_handler.h

```h
#ifndef AUDIO_HANDLER_H
#define AUDIO_HANDLER_H

#include <Arduino.h>

void init_audio();
void create_wav_header(byte *header, int data_size, int bitsPerSample);

#endif // AUDIO_HANDLER_H


```
---
## File: include/bluetooth_handler.h

```h
#ifndef BLUETOOTH_HANDLER_H
#define BLUETOOTH_HANDLER_H

void start_bluetooth();
void handle_capture_cycle();

#endif // BLUETOOTH_HANDLER_H
```
---
## File: include/camera_handler.h

```h
#ifndef CAMERA_HANDLER_H
#define CAMERA_HANDLER_H

void init_camera();

#endif // CAMERA_HANDLER_H
```
---
## File: include/display_handler.h

```h
#ifndef DISPLAY_HANDLER_H
#define DISPLAY_HANDLER_H

void init_display();
void update_display(int line, const char *text, bool do_display = true);
void update_mode_display();

#endif // DISPLAY_HANDLER_H
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
```
---
## File: include/hardware_handler.h

```h
#ifndef HARDWARE_HANDLER_H
#define HARDWARE_HANDLER_H

#include "globals.h"

extern portMUX_TYPE buttonMux;

void init_hardware();
void IRAM_ATTR on_timer();
void IRAM_ATTR handle_button_press();

#endif // HARDWARE_HANDLER_H
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
build_flags =
    -D BOARD_HAS_PSRAM
    -D CAMERA_MODEL_TTGO_T_CAMERA_V162
    -D BLE_DEVICE_NAME="\"T-Camera-BLE\""

; 3. Ensure we have enough space for the large camera application firmware.
board_build.partitions = huge_app.csv
```
---
## File: src/audio_handler.cpp

```cpp
#include "globals.h"
#include "display_handler.h"
#include "driver/i2s.h"

void init_audio()
{
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = 64,
        .use_apll = false};
    if (i2s_driver_install(I2S_NUM_1, &i2s_config, 0, NULL) != ESP_OK)
    {
        update_display(1, "Mic FAIL");
        Serial.println("Mic Init Failed!");
        return;
    }
    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_SCK,
        .ws_io_num = I2S_WS,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = I2S_SD};
    i2s_set_pin(I2S_NUM_1, &pin_config);
    update_display(1, "Mic Init OK");
    Serial.println("Mic Initialized.");
}

void create_wav_header(byte *header, int data_size, int bitsPerSample)
{
    int32_t sampleRate = SAMPLE_RATE;
    int16_t numChannels = 1;
    int16_t blockAlign = numChannels * bitsPerSample / 8;
    int32_t byteRate = sampleRate * blockAlign;
    int32_t subchunk2Size = data_size;
    int32_t chunkSize = 36 + subchunk2Size;

    header[0] = 'R';
    header[1] = 'I';
    header[2] = 'F';
    header[3] = 'F';
    *((int32_t *)(header + 4)) = chunkSize;
    header[8] = 'W';
    header[9] = 'A';
    header[10] = 'V';
    header[11] = 'E';
    header[12] = 'f';
    header[13] = 'm';
    header[14] = 't';
    header[15] = ' ';
    *((int32_t *)(header + 16)) = 16;
    *((int16_t *)(header + 20)) = 1; // PCM
    *((int16_t *)(header + 22)) = numChannels;
    *((int32_t *)(header + 24)) = sampleRate;
    *((int32_t *)(header + 28)) = byteRate;
    *((int16_t *)(header + 32)) = blockAlign;
    *((int16_t *)(header + 34)) = bitsPerSample;
    header[36] = 'd';
    header[37] = 'a';
    header[38] = 't';
    header[39] = 'a';
    *((int32_t *)(header + 40)) = subchunk2Size;
}
```
---
## File: src/bluetooth_handler.cpp

```cpp
// ... (includes and other functions are the same as the previous response) ...
#include "globals.h"
#include "bluetooth_handler.h"
#include "display_handler.h"
#include "audio_handler.h"
#include "driver/i2s.h"
#include <BLE2902.h>

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
            Serial.printf("CMD onWrite: %c\n", cmd); // <-- helpful debug log
            if (cmd == 'N')                          // "Next" chunk
            {
                next_chunk_requested = true;
            }
            else if (cmd == 'C') // "Complete"
            {
                transfer_complete = true;
                Serial.println("Server acknowledged completion. Ready for next trigger.");
            }
            else if (cmd == 'T') // "Trigger" capture
            {
                if (transfer_complete)
                {
                    Serial.println("DEBUG: Received TRIGGER signal from server.");
                    capture_requested = true;
                }
                else
                {
                    Serial.println("WARN: Ignoring trigger, transfer already in progress.");
                }
            }
        }
    }
};

// MyServerCallbacks, start_bluetooth, notify_chunk and wait_for_next_chunk_request are correct from the previous attempt and do not need changes.
class MyServerCallbacks : public BLEServerCallbacks
{
    void onConnect(BLEServer *pServer)
    {
        client_connected = true;
        transfer_complete = true; // Ready for a trigger command
        update_mode_display();
        Serial.println("Client Connected. Waiting for Trigger command...");
    }

    void onDisconnect(BLEServer *pServer)
    {
        client_connected = false;
        transfer_complete = true; // Reset state
        update_mode_display();
        Serial.println("Client Disconnected");
        BLEDevice::startAdvertising();
    }
};

void start_bluetooth()
{
    Serial.println("Starting BLE server...");
    BLEDevice::init(BLE_DEVICE_NAME);
    BLEDevice::setMTU(517);

    BLEServer *pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());
    pServer->getAdvertising()->setMinInterval(0x20);
    pServer->getAdvertising()->setMaxInterval(0x40);

    BLEService *pService = pServer->createService(SERVICE_UUID);

    pStatusCharacteristic = pService->createCharacteristic(CHARACTERISTIC_UUID_STATUS, BLECharacteristic::PROPERTY_NOTIFY);
    pStatusCharacteristic->addDescriptor(new BLE2902());

    pDataCharacteristic = pService->createCharacteristic(CHARACTERISTIC_UUID_DATA, BLECharacteristic::PROPERTY_NOTIFY);
    pDataCharacteristic->addDescriptor(new BLE2902());

    // pCommandCharacteristic = pService->createCharacteristic(CHARACTERISTIC_UUID_COMMAND, BLECharacteristic::PROPERTY_WRITE);
    // pCommandCharacteristic->setCallbacks(new CommandCallbacks());
    // Accept both "Write (with response)" and "Write Without Response" for reliability
    pCommandCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_COMMAND,
        BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);
    pCommandCharacteristic->setCallbacks(new CommandCallbacks());

    pService->start();

    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    BLEDevice::startAdvertising();

    Serial.println("Bluetooth Initialized.");
    update_display(3, "BLE Init OK");
}
void notify_chunk(const uint8_t *data, size_t size)
{
    pDataCharacteristic->setValue((uint8_t *)data, size);
    pDataCharacteristic->notify();
}
bool wait_for_next_chunk_request(uint32_t timeout_ms)
{
    uint32_t start = millis();
    while (millis() - start < timeout_ms)
    {
        if (next_chunk_requested)
        {
            next_chunk_requested = false;
            return true;
        }
        vTaskDelay(5 / portTICK_PERIOD_MS);
    }
    return false;
}

bool send_data_with_flow_control(const uint8_t *buffer, size_t total_size, const char *data_type)
{
    if (total_size == 0)
        return true;

    Serial.printf("Starting %s transfer (%u bytes)...\n", data_type, total_size);

    size_t sent = 0;

    // --- FIX: Push the first chunk immediately without waiting for a request ---
    size_t remaining = total_size - sent;
    size_t chunk_size = (remaining < CHUNK_SIZE) ? remaining : CHUNK_SIZE;
    Serial.printf("Pushing first %s chunk (%u bytes)...\n", data_type, chunk_size);
    notify_chunk(buffer + sent, chunk_size);
    sent += chunk_size;
    // --- END FIX ---

    // For all subsequent chunks, wait for the server's "Next" command
    while (sent < total_size)
    {
        if (!wait_for_next_chunk_request(15000)) // 15s timeout
        {
            Serial.printf("ERROR: Timeout waiting for %s chunk request at byte %u/%u\n",
                          data_type, sent, total_size);
            return false;
        }

        remaining = total_size - sent;
        chunk_size = (remaining < CHUNK_SIZE) ? remaining : CHUNK_SIZE;

        notify_chunk(buffer + sent, chunk_size);
        sent += chunk_size;
    }

    Serial.printf("%s transfer complete (%u bytes sent).\n", data_type, sent);
    return true;
}

void handle_capture_cycle()
{
    if (!client_connected)
        return;

    // Correctly check the mode set by the hardware button
    bool do_image = (current_mode == MODE_BOTH || current_mode == MODE_IMAGE_ONLY);
    bool do_audio = (current_mode == MODE_BOTH || current_mode == MODE_AUDIO_ONLY);
    bool transfer_ok = true;

    camera_fb_t *fb = NULL;
    uint32_t img_size = 0;
    if (do_image)
    {
        fb = esp_camera_fb_get();
        if (!fb)
        {
            Serial.println("Camera capture failed");
            pStatusCharacteristic->setValue("0:0"); // Send empty status
            pStatusCharacteristic->notify();
            return;
        }
        img_size = fb->len;
        Serial.printf("Captured image: %u bytes\n", img_size);
    }

    uint32_t total_audio_size = 0;
    byte *audio_buffer = NULL;
    if (do_audio)
    {
        const int num_samples = RECORD_DURATION * SAMPLE_RATE;
        const uint32_t audio_data_size = num_samples * 16 / 8;
        total_audio_size = HEADER_SIZE + audio_data_size;

        audio_buffer = (byte *)malloc(total_audio_size);
        if (!audio_buffer)
        {
            Serial.println("Failed to allocate memory for audio buffer!");
            total_audio_size = 0; // Ensure we don't try to send it
        }
        else
        {
            create_wav_header(audio_buffer, audio_data_size, 16);
            size_t bytes_read = 0;
            i2s_read(I2S_NUM_1, audio_buffer + HEADER_SIZE, audio_data_size, &bytes_read, portMAX_DELAY);
            Serial.printf("Recorded audio: %u bytes\n", total_audio_size);
        }
    }

    // Send status notification. The server is guaranteed to be ready.
    char status_buf[32];
    sprintf(status_buf, "%u:%u", img_size, total_audio_size);
    pStatusCharacteristic->setValue(status_buf);
    pStatusCharacteristic->notify();
    Serial.printf("Sent STATUS: %s. Starting transfer...\n", status_buf);

    if (do_image && img_size > 0)
    {
        if (!send_data_with_flow_control(fb->buf, fb->len, "image"))
        {
            transfer_ok = false;
        }
    }
    if (fb)
        esp_camera_fb_return(fb);

    if (transfer_ok && do_audio && total_audio_size > 0)
    {
        if (!send_data_with_flow_control(audio_buffer, total_audio_size, "audio"))
        {
            transfer_ok = false;
        }
    }
    if (audio_buffer)
        free(audio_buffer);

    if (!transfer_ok)
    {
        // If the transfer fails, reset state so the server can re-trigger
        Serial.println("Transfer failed mid-stream. Resetting state.");
        transfer_complete = true;
    }
    // Otherwise, wait for the server's 'C' command to set transfer_complete = true
}
```
---
## File: src/camera_handler.cpp

```cpp
#include "globals.h"
#include "display_handler.h" // For updating display during init

void init_camera()
{
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
    config.pin_sccb_sda = SIOD_GPIO_NUM;
    config.pin_sccb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;

    if (psramFound())
    {
        config.frame_size = FRAMESIZE_VGA;
        config.jpeg_quality = 12;
        config.fb_count = 2;
        config.fb_location = CAMERA_FB_IN_PSRAM;
    }
    else
    {
        config.frame_size = FRAMESIZE_SVGA;
        config.jpeg_quality = 12;
        config.fb_count = 1;
        config.fb_location = CAMERA_FB_IN_DRAM;
    }

    if (esp_camera_init(&config) != ESP_OK)
    {
        update_display(2, "Cam FAIL");
        Serial.println("Cam Init Failed!");
        return;
    }
    update_display(2, "Cam Init OK");
    Serial.println("Camera Initialized.");
}
```
---
## File: src/display_handler.cpp

```cpp
#include "globals.h"
#include "display_handler.h"

void init_display()
{
    Wire.begin(I2C_SDA, I2C_SCL);
    display.init();
    display.flipScreenVertically();
    display.setFont(ArialMT_Plain_10);
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.clear();
    update_display(0, "OLED Init OK");
    Serial.println("OLED Initialized.");
}

void update_display(int line, const char *text, bool do_display)
{
    display.drawString(0, line * 10, text);
    if (do_display)
        display.display();
}

void update_mode_display()
{
    display.clear();
    char mode_buf[32];
    sprintf(mode_buf, "Mode: %s", mode_names[(int)current_mode]);
    update_display(0, mode_buf, false);
    if (client_connected)
    {
        update_display(2, "Status: Connected", true);
    }
    else
    {
        update_display(2, "Status: Waiting...", true);
    }
}
```
---
## File: src/hardware_handler.cpp

```cpp
#include "hardware_handler.h"

portMUX_TYPE buttonMux = portMUX_INITIALIZER_UNLOCKED;
volatile unsigned long last_interrupt_time = 0;

void init_hardware()
{
    // Button setup for changing modes
    pinMode(BTN_GPIO_NUM, INPUT);
    attachInterrupt(digitalPinToInterrupt(BTN_GPIO_NUM), handle_button_press, FALLING);
}

void IRAM_ATTR handle_button_press()
{
    portENTER_CRITICAL_ISR(&buttonMux);
    unsigned long interrupt_time = millis();
    if (interrupt_time - last_interrupt_time > 200)
    { // 200ms debounce
        current_mode = (CaptureMode)(((int)current_mode + 1) % MODE_COUNT);
        mode_changed = true;
    }
    last_interrupt_time = interrupt_time;
    portEXIT_CRITICAL_ISR(&buttonMux);
}
```
---
## File: src/main.cpp

```cpp
#include "globals.h"
#include "hardware_handler.h" // For button handling

// Define global objects
SSD1306 display(0x3c, I2C_SDA, I2C_SCL, GEOMETRY_128_64);

// Define global state flags
volatile bool capture_requested = false;
volatile bool client_connected = false;
volatile CaptureMode current_mode = MODE_IMAGE_ONLY;
volatile bool mode_changed = true; // Set to true to display initial mode
volatile bool next_chunk_requested = false;
volatile bool transfer_complete = true; // Start in a ready state

const char *mode_names[] = {"Send Both", "Image Only", "Audio Only"};

void setup()
{
  Serial.begin(115200);
  Serial.println("\n\n--- T-Camera Refactored BLE v3.1 (Server-Driven) ---");

  init_hardware(); // Sets up the button
  init_display();
  init_camera();
  init_audio();
  start_bluetooth();

  Serial.println("Setup Complete. Waiting for BLE client...");
}

void loop()
{
  // Check if the physical button was pressed to change modes
  if (mode_changed)
  {
    portENTER_CRITICAL(&buttonMux);
    mode_changed = false;
    portEXIT_CRITICAL(&buttonMux);

    Serial.printf("Mode -> %s\n", mode_names[(int)current_mode]);
    update_mode_display();
  }

  // Check if the server has requested a capture cycle
  if (capture_requested)
  {
    // A capture cycle has been requested by the server
    transfer_complete = false; // Mark that a transfer is now active
    capture_requested = false; // Reset the request flag

    handle_capture_cycle();

    // After this, the device waits for the server to send a 'C' (Complete) command.
    // The BLE callback will see this and set transfer_complete = true,
    // making the device ready for the next trigger from the server.
  }

  // Small delay to prevent busy-waiting
  delay(10);
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
from flask import Flask, render_template, send_from_directory
from bleak import BleakScanner, BleakClient

# --- CONFIGURATION ---
RECONNECT_DELAY_SECONDS = 5
CAPTURE_INTERVAL_SECONDS = 2  # How often to request a capture
FLASK_PORT = 5550
DEVICE_NAME = "T-Camera-BLE"

# --- BLE UUIDS ---
SERVICE_UUID = "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
CHARACTERISTIC_UUID_STATUS = "beb5483e-36e1-4688-b7f5-ea07361b26a8"
CHARACTERISTIC_UUID_DATA = "7347e350-5552-4822-8243-b8923a4114d2"
CHARACTERISTIC_UUID_COMMAND = "a244c201-1fb5-459e-8fcc-c5c9c331914b"

# --- PROTOCOL COMMANDS ---
CMD_NEXT_CHUNK = b'N'
CMD_TRANSFER_COMPLETE = b'C'
CMD_TRIGGER_CAPTURE = b'T'

# --- WEB SERVER & GLOBAL STATE ---
app = Flask(__name__)
STATIC_FOLDER = 'static'
LATEST_IMAGE_FILENAME = 'latest.jpg'
LATEST_AUDIO_FILENAME = 'latest.wav'
LATEST_IMAGE_PATH = os.path.join(STATIC_FOLDER, LATEST_IMAGE_FILENAME)
LATEST_AUDIO_PATH = os.path.join(STATIC_FOLDER, LATEST_AUDIO_FILENAME)
server_state = {"status": "Initializing..."}
transfer_in_progress = asyncio.Event()
data_queue = asyncio.Queue()

# Ensure static folder exists
if not os.path.exists(STATIC_FOLDER):
    os.makedirs(STATIC_FOLDER)


async def transfer_data(client, expected_size, buffer, data_type):
    """
    Receives data from the ESP32. It waits for the first chunk to be "pushed"
    by the device, then "pulls" the remaining chunks.
    """
    if expected_size == 0:
        return True

    print(f"Starting {data_type} transfer. Expecting {expected_size} bytes.")
    bytes_received = 0

    # --- FIX: Wait for the first chunk to be "pushed" by the ESP32 ---
    try:
        print("Waiting for the first pushed chunk...")
        first_chunk = await asyncio.wait_for(data_queue.get(), timeout=15.0)
        buffer.extend(first_chunk)
        bytes_received = len(buffer)
        data_queue.task_done()
        print(f"Received first pushed chunk ({len(first_chunk)} bytes).")
    except asyncio.TimeoutError:
        print(
            f"ERROR: Timeout waiting for the first pushed {data_type} data chunk.")
        return False
    except Exception as e:
        print(f"ERROR receiving first chunk: {e}")
        return False
    # --- END FIX ---

    # Now, pull the rest of the data
    while bytes_received < expected_size:
        try:
            await client.write_gatt_char(CHARACTERISTIC_UUID_COMMAND, CMD_NEXT_CHUNK, response=False)
            chunk = await asyncio.wait_for(data_queue.get(), timeout=15.0)
            buffer.extend(chunk)
            bytes_received = len(buffer)
            data_queue.task_done()
        except asyncio.TimeoutError:
            print(
                f"ERROR: Timeout waiting for {data_type} data chunk at {bytes_received}/{expected_size} bytes.")
            return False
        except Exception as e:
            print(f"ERROR in transfer_data loop: {e}")
            return False

    print(f"-> {data_type} transfer complete ({bytes_received} bytes received).")
    return True


def data_notification_handler(sender, data):
    """Callback that puts received data chunks into the async queue."""
    data_queue.put_nowait(data)


async def status_notification_handler(sender, data, client):
    """
    Callback that handles the initial status update, then starts the
    data transfer process for image and audio.
    """
    try:
        status_str = data.decode('utf-8')
        img_size_str, audio_size_str = status_str.split(':')
        img_size = int(img_size_str)
        audio_size = int(audio_size_str)

        print(
            f"\n[STATUS] Received: Image: {img_size} bytes, Audio: {audio_size} bytes.")
        server_state["status"] = f"Receiving: Img({img_size}B) Aud({audio_size}B)"

        # Transfer Image data
        img_buffer = bytearray()
        if not await transfer_data(client, img_size, img_buffer, "Image"):
            print("Image transfer failed.")
        elif img_size > 0:
            with open(LATEST_IMAGE_PATH, "wb") as f:
                f.write(img_buffer)
            print(f"-> Saved latest image to {LATEST_IMAGE_PATH}")

        # Transfer Audio data
        audio_buffer = bytearray()
        if not await transfer_data(client, audio_size, audio_buffer, "Audio"):
            print("Audio transfer failed.")
        elif audio_size > 0:
            with open(LATEST_AUDIO_PATH, "wb") as f:
                f.write(audio_buffer)
            print(f"-> Saved latest audio to {LATEST_AUDIO_PATH}")

    except Exception as e:
        print(f"Error in status_notification_handler: {e}")
    finally:
        # This is CRITICAL: Always acknowledge completion to unblock the ESP32.
        print("--> Sending 'Complete' acknowledgement to ESP32.")
        try:
            await client.write_gatt_char(CHARACTERISTIC_UUID_COMMAND, CMD_TRANSFER_COMPLETE, response=False)
        except Exception as e:
            print(f"Warning: Could not send 'Complete' ack: {e}")

        print("\n--- Cycle Complete ---")
        # Signal to the main loop that this cycle is done.
        transfer_in_progress.set()


async def ble_communication_task():
    """The main async task that connects to the ESP32 and drives the capture loop."""
    while True:
        try:
            server_state["status"] = f"Scanning for {DEVICE_NAME}..."
            print(server_state["status"])
            device = await BleakScanner.find_device_by_name(DEVICE_NAME, timeout=10.0)

            if not device:
                await asyncio.sleep(RECONNECT_DELAY_SECONDS)
                continue

            async with BleakClient(device, timeout=20.0) as client:
                if client.is_connected:
                    print(f"Successfully connected to {device.address}")
                    server_state["status"] = "Connected"

                    # Register notification handlers
                    status_handler_with_client = functools.partial(
                        status_notification_handler, client=client)
                    await client.start_notify(CHARACTERISTIC_UUID_STATUS, status_handler_with_client)
                    await client.start_notify(CHARACTERISTIC_UUID_DATA, data_notification_handler)

                    # Main server-driven capture loop
                    while client.is_connected:
                        print(f"Triggering new capture...")
                        server_state["status"] = "Triggering Capture..."
                        transfer_in_progress.clear()

                        await client.write_gatt_char(CHARACTERISTIC_UUID_COMMAND, CMD_TRIGGER_CAPTURE, response=True)

                        # Wait for the status_handler to signal completion via the event
                        await asyncio.wait_for(transfer_in_progress.wait(), timeout=60.0)

                        server_state["status"] = "Idle, waiting for next cycle..."
                        await asyncio.sleep(CAPTURE_INTERVAL_SECONDS)

        except asyncio.TimeoutError:
            print("Timeout during capture cycle. Retrying...")
            server_state["status"] = "Timeout. Retrying..."
        except Exception as e:
            print(f"An error occurred in ble_communication_task: {e}")
            server_state["status"] = "Connection Lost. Retrying..."
        finally:
            print(
                f"Disconnected. Retrying in {RECONNECT_DELAY_SECONDS} seconds...")
            await asyncio.sleep(RECONNECT_DELAY_SECONDS)

# --- Flask Web Server Setup ---


@app.route('/')
def index():
    """Serves the main HTML page."""
    return render_template('index.html', status=server_state.get("status"))


@app.route(f'/{LATEST_IMAGE_FILENAME}')
def latest_image():
    """Serves the latest captured image."""
    return send_from_directory(STATIC_FOLDER, LATEST_IMAGE_FILENAME, as_attachment=False, mimetype='image/jpeg')


def run_flask_app():
    """Runs the Flask app in a separate thread."""
    print(f"Starting Flask web server on http://0.0.0.0:{FLASK_PORT}")
    app.run(host='0.0.0.0', port=FLASK_PORT, debug=False)


if __name__ == '__main__':
    # Start the Flask server in a background thread
    flask_thread = threading.Thread(target=run_flask_app, daemon=True)
    flask_thread.start()

    # Start the main BLE communication loop
    print("Starting BLE communication task...")
    try:
        asyncio.run(ble_communication_task())
    except KeyboardInterrupt:
        print("Program stopped by user.")

```
---
## File: srv/reqs.txt

```txt
flask
bleak
```
---
