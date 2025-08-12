#ifndef DISPLAY_HANDLER_H
#define DISPLAY_HANDLER_H

void init_display();
void update_display(int line, const char *text, bool do_display = true);
void update_mode_display();

#endif // DISPLAY_HANDLER_H