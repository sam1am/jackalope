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