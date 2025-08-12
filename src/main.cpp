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