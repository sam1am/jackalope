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