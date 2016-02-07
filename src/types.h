#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef char* string_t;

typedef uintptr_t* window_t;
typedef uintptr_t* screen_t;

typedef uintptr_t* hotkey_t;
typedef uintptr_t* modkeys_t;
typedef uintptr_t* keys_t;

typedef void (*cb_enumerate_windows_t)(window_t w);

typedef struct {
    int32_t left, right, top, bottom;
} rect_t;

typedef struct {
    int32_t x, y;
} pos_t;

typedef struct {
    window_t   window;
    rect_t     rect;
    uintptr_t* prev; // node_t
    uintptr_t* next; // node_t
} node_t;
