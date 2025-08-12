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
        // FIX: Added PROPERTY_WRITE_NR to allow the server to send settings without a response.
        BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);
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
}