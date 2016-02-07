#pragma once

#include "types.h"

bool driver_init();
void driver_destroy();
bool driver_main();

int32_t driver_get_screen_count();
screen_t driver_get_screen(int32_t i);
rect_t driver_get_screen_rect(screen_t s);

rect_t driver_get_window_rect(window_t w);
void driver_set_window_rect(window_t w, rect_t r);
void driver_set_window_visible(window_t w, bool v);
window_t driver_get_foreground_window();
void driver_set_foreground_window(window_t w);
string_t driver_get_window_name(window_t w);

pos_t driver_get_cursor_position();
void driver_set_cursor_position(pos_t p);
void driver_set_cursor_rect(rect_t r);

bool driver_should_tile_window_p(window_t w);
void driver_enumerate_windows(cb_enumerate_windows_t f);

void driver_hotkey_add(hotkey_t h, modkeys_t m, keys_t k);
void driver_hotkey_remove(hotkey_t h);

void driver_message(string_t title, string_t msg);
