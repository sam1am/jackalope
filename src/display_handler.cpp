#include "globals.h"
#include "display_handler.h"

// Extern the global display object defined in main.cpp
extern SSD1306 display;

void init_display()
{
    // Re-initializing after deep sleep is safe and necessary.
    Wire.begin(I2C_SDA, I2C_SCL);
    display.init();
    display.flipScreenVertically();
    display.setFont(ArialMT_Plain_10);
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.clear();
    update_display(0, "OLED Init OK", true);
    Serial.println("OLED Initialized.");
}

void update_display(int line, const char *text, bool do_display)
{
    // Clear the line before writing new text to prevent artifacts
    display.setColor(BLACK);
    display.fillRect(0, line * 10, 128, 10);
    display.setColor(WHITE);
    display.drawString(0, line * 10, text);
    if (do_display)
        display.display();
}