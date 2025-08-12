#include "hardware_handler.h"

portMUX_TYPE buttonMux = portMUX_INITIALIZER_UNLOCKED;
volatile unsigned long last_interrupt_time = 0;

void init_hardware()
{
    // Button setup for changing modes
    pinMode(BTN_GPIO_NUM, INPUT);
    attachInterrupt(digitalPinToInterrupt(BTN_GPIO_NUM), handle_button_press, FALLING);
}

void IRAM_ATTR handle_button_press()
{
    portENTER_CRITICAL_ISR(&buttonMux);
    unsigned long interrupt_time = millis();
    if (interrupt_time - last_interrupt_time > 200)
    { // 200ms debounce
        current_mode = (CaptureMode)(((int)current_mode + 1) % MODE_COUNT);
        mode_changed = true;
    }
    last_interrupt_time = interrupt_time;
    portEXIT_CRITICAL_ISR(&buttonMux);
}