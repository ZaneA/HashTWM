#pragma once

#include <string.h>
#include <stdint.h>
#include <stdbool.h>

typedef char* string_t;

typedef uintptr_t* window_t;
typedef uintptr_t* screen_t;

typedef uintptr_t* hotkey_t;
typedef uintptr_t* modkeys_t;
typedef char keys_t;

typedef void (*cb_enumerate_windows_t)(window_t w);

typedef struct {
    int32_t left, right, top, bottom;
} rect_t;

typedef struct {
    int32_t x, y;
} pos_t;

typedef struct node_s {
    window_t   window;
    rect_t     rect;
    struct node_s* prev; // node_t
    struct node_s* next; // node_t
} node_s;

typedef node_s* node_t;
