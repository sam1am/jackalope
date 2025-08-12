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