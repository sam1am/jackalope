#include "globals.h"
#include "display_handler.h"
#include "esp_heap_caps.h"
#include <cstring>

// --- GLOBAL OBJECTS & VARIABLE DEFINITIONS ---
SSD1306 display(0x3c, I2C_SDA, I2C_SCL, GEOMETRY_128_64);
Preferences preferences;

// Configuration settings with defaults
int deep_sleep_seconds = 10;
float storage_threshold_percent = 5.0;

// Global state flags
volatile bool client_connected = false;
volatile bool next_chunk_requested = false;
volatile bool transfer_acknowledged = false;
volatile bool new_config_received = false;
char pending_config_str[64];

// Forward declaration from bluetooth_handler.cpp, where these are defined
extern uint8_t *framebuffers[IMAGE_BATCH_SIZE];
extern size_t fb_lengths[IMAGE_BATCH_SIZE];
extern int image_count;

// src/main.cpp

// FIX: Modified sleep function to ensure serial stability before and after sleep
void enter_light_sleep(int sleep_time_seconds)
{
  Serial.printf("Entering light sleep for %d seconds.\n", sleep_time_seconds);

  // 1. Power down peripherals
  stop_bluetooth();
  display.displayOff(); // Turning off display also powers down camera on this board

  // 2. Configure wakeup source
  esp_sleep_enable_timer_wakeup(sleep_time_seconds * 1000000ULL);

  // ADDED: Wait for the serial transmit buffer to empty before sleeping.
  // This prevents the "BLE Sto..." cutoff issue.
  Serial.flush();

  // 3. Enter light sleep
  esp_light_sleep_start();

  // --- WAKE UP ---

  // ADDED: A small delay to allow the serial port to stabilize after waking up.
  // This prevents the garbled text issue.
  delay(100);

  Serial.println("\nWoke up from light sleep."); // Added a newline for cleaner logs

  // 4. Re-initialize peripherals
  display.displayOn();
  init_display();
  start_bluetooth(); // Restart BLE to be ready for connections/commands
  update_display(0, "System Ready", true);
}

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

// FIX: This function now works again because BLE is active during the loop.
void apply_new_settings()
{
  Serial.printf("Applying new settings: '%s'\n", pending_config_str);

  char *f_part = strstr(pending_config_str, "F:");
  if (f_part)
  {
    int new_freq = atoi(f_part + 2);
    if (new_freq > 0)
    {
      deep_sleep_seconds = new_freq;
      Serial.printf("Parsed Frequency: %d\n", new_freq);
    }
  }

  char *t_part = strstr(pending_config_str, "T:");
  if (t_part)
  {
    float new_thresh = atof(t_part + 2);
    if (new_thresh >= 10 && new_thresh <= 95)
    {
      storage_threshold_percent = new_thresh;
      Serial.printf("Parsed Threshold: %.1f\n", new_thresh);
    }
  }

  preferences.begin("settings", false);
  preferences.putInt("sleep_sec", deep_sleep_seconds);
  preferences.putFloat("storage_pct", storage_threshold_percent);
  preferences.end();

  Serial.printf("Settings saved and applied: Interval=%ds, Threshold=%.1f%%\n", deep_sleep_seconds, storage_threshold_percent);
  update_display(4, "Settings Saved!", true);
  delay(1500);
  update_display(4, "", true);

  new_config_received = false;
}

// --- SETUP: Runs once at power-on ---
void setup()
{
  Serial.begin(115200);
  Serial.println("\n--- T-Camera Continuous Timelapse (Low Power) ---");

  setCpuFrequencyMhz(80);
  Serial.printf("CPU Freq set to %d MHz\n", getCpuFrequencyMhz());

  init_display();
  load_settings();
  init_camera();
  start_bluetooth(); // Start BLE on initial boot.

  update_display(0, "System Ready", true);
  Serial.println("System initialized and running. Waiting for first capture interval.");
}

// --- LOOP: Main program cycle ---
void loop()
{
  // Check for settings first, as BLE is guaranteed to be on.
  if (new_config_received)
  {
    apply_new_settings();
  }

  enter_light_sleep(deep_sleep_seconds);

  setCpuFrequencyMhz(240);
  Serial.printf("CPU Freq set to %d MHz for capture\n", getCpuFrequencyMhz());

  Serial.println("Capture interval elapsed. Taking picture...");
  if (!store_image_in_psram())
  {
    Serial.println("Failed to store image. Check PSRAM or batch size limit.");
  }

  setCpuFrequencyMhz(80);
  Serial.printf("CPU Freq returned to %d MHz\n", getCpuFrequencyMhz());

  size_t total_psram = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
  size_t free_psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
  float used_percentage = total_psram > 0 ? (1.0 - ((float)free_psram / total_psram)) * 100.0 : 0;

  char status_buf[40];
  sprintf(status_buf, "PSRAM: %.1f%% | Imgs: %d", used_percentage, image_count);
  Serial.println(status_buf);
  update_display(1, status_buf, true);

  if (client_connected && pStatusCharacteristic != NULL)
  {
    pStatusCharacteristic->setValue(status_buf);
    pStatusCharacteristic->notify();
  }

  bool should_transfer = (image_count > 0 && (used_percentage >= storage_threshold_percent || image_count >= IMAGE_BATCH_SIZE));

  if (should_transfer)
  {
    setCpuFrequencyMhz(240);
    Serial.printf("CPU Freq boosted to %d MHz for transfer\n", getCpuFrequencyMhz());

    Serial.printf("Transfer condition met (Usage: %.1f%%, Count: %d).\n", used_percentage, image_count);
    bool transfer_attempted = false;

    if (client_connected)
    {
      Serial.println("Client is already connected. Starting transfer.");
      update_display(2, "Connected! Sending...", true);
      delay(500); // Wait for server to be ready before sending data
      send_batched_data();
      transfer_attempted = true;
    }
    else
    {
      Serial.println("Waiting for a client to connect for transfer...");
      update_display(2, "Batch full. Wait conn.", true);
      uint32_t start_time = millis();
      while (!client_connected && (millis() - start_time < 30000))
      {
        delay(100);
      }
      if (client_connected)
      {
        Serial.println("Client connected for transfer.");
        update_display(2, "Connected! Sending...", true);
        delay(500); // Wait for server to be ready before sending data
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
      Serial.println("Waiting for BLE TX buffer to clear...");
      delay(500);
      clear_image_buffers();
    }

    update_display(2, "", true);

    setCpuFrequencyMhz(80);
    Serial.printf("CPU Freq returned to %d MHz\n", getCpuFrequencyMhz());
  }
}