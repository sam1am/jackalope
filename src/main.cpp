#include "globals.h"
#include "display_handler.h"
#include "esp_heap_caps.h"

// --- GLOBAL OBJECTS & VARIABLE DEFINITIONS ---
SSD1306 display(0x3c, I2C_SDA, I2C_SCL, GEOMETRY_128_64);
Preferences preferences;

// Configuration settings with defaults
int deep_sleep_seconds = 5;
float storage_threshold_percent = 0.4;

// Global state flags
volatile bool client_connected = false;
volatile bool next_chunk_requested = false;
volatile bool transfer_acknowledged = false;

// Forward declaration from bluetooth_handler.cpp
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

// --- LOOP: Main program cycle (Corrected Logic) ---
void loop()
{
  // 1. Wait for the specified interval.
  Serial.printf("Waiting for %d seconds...\n", deep_sleep_seconds);
  delay(deep_sleep_seconds * 1000);

  // 2. Capture an image. This now happens even if a client is connected.
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

    // If a client is already connected, we can start the transfer immediately.
    if (client_connected)
    {
      Serial.println("Client is already connected. Starting transfer.");
      update_display(2, "Connected! Sending...", true);
      send_batched_data();
    }
    else
    {
      // Otherwise, wait for a new connection for a limited time.
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
        clear_image_buffers(); // Free up space to continue timelapse
      }
    }
    update_display(2, "", true); // Clear the status line after the attempt
  }
}