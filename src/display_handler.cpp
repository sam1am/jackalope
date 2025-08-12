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