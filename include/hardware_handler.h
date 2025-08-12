#ifndef HARDWARE_HANDLER_H
#define HARDWARE_HANDLER_H

#include "globals.h"

extern portMUX_TYPE buttonMux;

void init_hardware();
void IRAM_ATTR on_timer();
void IRAM_ATTR handle_button_press();

#endif // HARDWARE_HANDLER_H