#ifndef DISPLAY_HANDLER_H
#define DISPLAY_HANDLER_H

#include <Arduino.h> // Include for basic types if needed

void init_display();
void update_display(int line, const char *text, bool do_display = true);

#endif // DISPLAY_HANDLER_H