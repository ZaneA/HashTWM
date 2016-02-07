#define DRIVER_DUMMY true

#include "types.h"
#include "driver.h"

#include <stdlib.h>
#include <stdio.h>

int32_t driver_get_screen_count() {
    return 1;
}

screen_t driver_get_screen(int32_t i) {
    return NULL;
}

rect_t driver_get_screen_rect(screen_t s) {
    rect_t r = {
        .left = 0,
        .right = 200,
        .top = 0,
        .bottom = 200
    };

    return r;
}

rect_t driver_get_window_rect(window_t w) {
    rect_t rect = {
        .left = 0,
        .right = 100,
        .top = 0,
        .bottom = 100
    };

    return rect;
}

void driver_set_window_rect(window_t w, rect_t r) {
    // noop
}

void driver_set_window_visible(window_t w, bool v) {
    // noop
}

window_t driver_get_foreground_window() {
    return NULL;
}

void driver_set_foreground_window(window_t w) {
    // noop
}

string_t driver_get_window_name(window_t w) {
    string_t temp = (string_t)malloc(strlen("Hello, World "));
    sprintf(temp, "%s", "Hello, World");

    return temp;
}

pos_t driver_get_cursor_position() {
    pos_t p = {
        .x = 50,
        .y = 50
    };

    return p;
}

void driver_set_cursor_position(pos_t p) {
    // noop
}

void driver_set_cursor_rect(rect_t r) {
    // noop
}

bool driver_should_tile_window_p(window_t w) {
    return true;
}

void driver_enumerate_windows(cb_enumerate_windows_t f) {
    // noop
}

void driver_hotkey_add(hotkey_t h, modkeys_t m, keys_t k) {
    // noop
}

void driver_hotkey_remove(hotkey_t h) {
    // noop
}

void driver_message(string_t title, string_t msg) {
    printf("%s: %s\n", title, msg);
}

bool driver_init() {
    return true;
}

void driver_destroy() {
    // noop
}

bool driver_main() {
    return true;
}
