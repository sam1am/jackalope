#include "globals.h"
#include "display_handler.h"

// Stored config to be used for re-initialization after deep sleep
camera_config_t camera_config;

void init_camera()
{
    // Configure camera settings
    camera_config.ledc_channel = LEDC_CHANNEL_0;
    camera_config.ledc_timer = LEDC_TIMER_0;
    camera_config.pin_d0 = Y2_GPIO_NUM;
    camera_config.pin_d1 = Y3_GPIO_NUM;
    camera_config.pin_d2 = Y4_GPIO_NUM;
    camera_config.pin_d3 = Y5_GPIO_NUM;
    camera_config.pin_d4 = Y6_GPIO_NUM;
    camera_config.pin_d5 = Y7_GPIO_NUM;
    camera_config.pin_d6 = Y8_GPIO_NUM;
    camera_config.pin_d7 = Y9_GPIO_NUM;
    camera_config.pin_xclk = XCLK_GPIO_NUM;
    camera_config.pin_pclk = PCLK_GPIO_NUM;
    camera_config.pin_vsync = VSYNC_GPIO_NUM;
    camera_config.pin_href = HREF_GPIO_NUM;
    camera_config.pin_sccb_sda = SIOD_GPIO_NUM;
    camera_config.pin_sccb_scl = SIOC_GPIO_NUM;
    camera_config.pin_pwdn = PWDN_GPIO_NUM;
    camera_config.pin_reset = RESET_GPIO_NUM;
    camera_config.xclk_freq_hz = 20000000;
    camera_config.pixel_format = PIXFORMAT_JPEG;
    camera_config.frame_size = FRAMESIZE_VGA;
    camera_config.jpeg_quality = 12;

    if (psramFound())
    {
        camera_config.fb_count = 2;
        camera_config.fb_location = CAMERA_FB_IN_PSRAM;
    }
    else
    {
        camera_config.fb_count = 1;
        camera_config.fb_location = CAMERA_FB_IN_DRAM;
    }

    if (esp_camera_init(&camera_config) != ESP_OK)
    {
        update_display(2, "Cam FAIL");
        Serial.println("Cam Init Failed!");
        return;
    }
    update_display(2, "Cam Init OK");
    Serial.println("Camera Initialized.");
}

void deinit_camera()
{
    if (esp_camera_deinit() == ESP_OK)
    {
        Serial.println("Camera De-initialized.");
    }
    else
    {
        Serial.println("Failed to de-init camera.");
    }
}