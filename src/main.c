//
// vim: ts=4 expandtab sw=4
// HashTWM
// An automatic Tiling Window Manager for Windows/OSX/Linux
// Copyright 2008-2016, Zane Ashby, www.zaneashby.co.nz
//

#define NAME      "HashTWM"
#define VERSION   "HashTWM 1.1.0"

#include "types.h"
#include "driver.h"

#include <stdlib.h>
#include <stdio.h>

#define DEFAULT_MODKEY        MODIFIER_SUPER
#define MAX_IGNORE            16
#define DEFAULT_TILING_MODE   MODE_VERTICAL
#define TAGS                  9

// Keyboard controls
enum modifier_key_e {
    MODIFIER_CTRL,
    MODIFIER_ALT,
    MODIFIER_SHIFT,
    MODIFIER_SUPER
};

enum actions_e {
    ACTION_SELECT_UP = 1,
    ACTION_SELECT_DOWN,
    ACTION_MOVE_MAIN,
    ACTION_EXIT,
    ACTION_MARGIN_LEFT,
    ACTION_MARGIN_RIGHT,
    ACTION_IGNORE,
    ACTION_MOUSE_LOCK,
    ACTION_TILING_MODE,
    ACTION_MOVE_UP,
    ACTION_MOVE_DOWN,
    ACTION_DISP_CLASS,
    ACTION_TILE,
    ACTION_UNTILE,
    ACTION_INC_AREA,
    ACTION_DEC_AREA,
    ACTION_CLOSE_WIN,
    ACTION_SWITCH_T1 = 100,
    ACTION_TOGGLE_T1 = 200
};

// Tiling modes
enum tiling_modes_e {
    MODE_VERTICAL = 0,
    MODE_HORIZONTAL,
    MODE_GRID,
    MODE_FULLSCREEN,
    // Keep this at the end if adding tiling modes
    MODE_END
};

// Timer modes
enum timer_modes_e {
    TIMER_UPDATE_MOUSE = 0
};

// Tags / Workspaces
typedef struct
{
    node_t nodes; // List of nodes
    node_t last_node;
    node_t current_window;
    unsigned short tilingMode;
    // Xmonad style Master area count
    unsigned short masterarea_count;
} tag_t;

// Global variables
tag_t g_tags[TAGS];
unsigned short g_current_tag = 0;
int g_screen_x, g_screen_y, g_screen_width, g_screen_height;
unsigned short g_experimental_mouse = 0;
unsigned short g_mouse_pos_out = 0;
int g_margin = 120;
unsigned short g_disable_next = 0;
unsigned short g_lock_mouse = 0;
unsigned short g_alpha = 255;
unsigned short g_ignore_count = 0;
int g_modkeys = DEFAULT_MODKEY;
char g_ignore_classes[MAX_IGNORE][128]; // Exclude tiling from the classes in here
char g_include_classes[MAX_IGNORE][128]; // Only tile the classes in here
unsigned short g_include_count = 0;
unsigned short g_include_mode = 0; // Exclude by default
unsigned short g_one_tag_per_window = 0; // If 1, remove current tag_t when adding a new one

bool util_is_in_list(char **list, unsigned int length, window_t window) {
    string_t temp = driver_get_window_name(window);

    for (int i = 0; i < length; i++) {
        if (!strcmp(temp, list[i])) {
            free(temp); return true;
        }
    }

    free(temp);

    return false;
}


//
// Linked-list methods
//

node_t find_node(window_t window, unsigned short tag_t)
{
    node_t temp;
    node_t nodes = g_tags[tag_t].nodes;

    for (temp = nodes; temp; temp = temp->next) {
        if (temp->window == window) {
            return temp;
        }
    }

    return NULL;
}

node_t find_node_full(window_t window)
{
    unsigned short tag_t;
    node_t found;

    for (tag_t = 0; tag_t < TAGS; tag_t++) {
        found = find_node(window, tag_t);
        if (found) return found;
    }

    return NULL;
}

void add_node(window_t window, unsigned short tag_t)
{
    node_t new_node;

    if (find_node(window, tag_t)) return;

    new_node = (node_t)malloc(sizeof(node_s));
    new_node->window = window;
    new_node->prev = NULL;
    new_node->next = NULL;

    // Save window layout
    new_node->rect = driver_get_window_rect(window);

    if (g_tags[tag_t].nodes == NULL) {
        new_node->prev = new_node;
        g_tags[tag_t].nodes = new_node;
        g_tags[tag_t].current_window = new_node;
        g_tags[tag_t].last_node = new_node;
    } else {
        g_tags[tag_t].last_node->next = new_node;
        new_node->prev = g_tags[tag_t].last_node;
        g_tags[tag_t].last_node = new_node;
        g_tags[tag_t].nodes->prev = new_node;
    }
}

void remove_node(window_t window, unsigned short tag_t)
{
    node_t temp;
    temp = find_node(window, tag_t);

    if (!temp) return;

    // Restore window layout
    driver_set_window_rect(window, temp->rect);

    // Remove the only node_t
    if (g_tags[tag_t].nodes == g_tags[tag_t].last_node) {
        g_tags[tag_t].nodes = NULL;
        g_tags[tag_t].last_node = NULL;
        g_tags[tag_t].current_window = NULL;
        // Remove the first node_t
    } else if (temp == g_tags[tag_t].nodes) {
        g_tags[tag_t].nodes = temp->next;
        g_tags[tag_t].nodes->prev = g_tags[tag_t].last_node;
        // Remove the last node_t
    } else if (temp == g_tags[tag_t].last_node) {
        g_tags[tag_t].last_node = temp->prev;
        g_tags[tag_t].nodes->prev = temp->prev;
        g_tags[tag_t].last_node->next = NULL;
        // Remove any other node_t
    } else {
        ((node_t)temp->prev)->next = temp->next;
        ((node_t)temp->next)->prev = temp->prev;
    }

    if (g_tags[tag_t].current_window == temp)
    g_tags[tag_t].current_window = temp->prev;

    free(temp);

    return;
}

void remove_node_full(window_t window)
{
    unsigned short tag_t;

    for (tag_t=0; tag_t<TAGS; tag_t++) {
        remove_node(window, tag_t);
    }
}

node_t find_node_in_chain(node_t head, int idx) {
    node_t nd = head;

    if (!head) {
        return NULL;
    }

    for (int i = 0; i < idx && nd; i++) {
        nd = (node_t)nd->next;
    }

    return nd;
}

void swap_window_with_node(node_t window)
{

    if (g_tags[g_current_tag].current_window == window) return;
    if (g_tags[g_current_tag].current_window && window) {
        window_t temp = window->window;
        window->window = g_tags[g_current_tag].current_window->window;
        g_tags[g_current_tag].current_window->window = temp;
        g_tags[g_current_tag].current_window = window;
    }
}

void swap_window_with_first_non_master_window() {
    node_t head = g_tags[g_current_tag].nodes;
    node_t current = g_tags[g_current_tag].current_window;
    int sub_node_idx = g_tags[g_current_tag].masterarea_count;

    if (current != head) {
        node_t nd = find_node_in_chain(head, sub_node_idx);
        swap_window_with_node(nd);
    }
}

void focus_current()
{
    node_t current = g_tags[g_current_tag].current_window;

    if (current) {
        driver_set_foreground_window(current->window);

        if (g_lock_mouse) {
            rect_t window = driver_get_window_rect(current->window);
            driver_set_cursor_rect(window);
            pos_t p;
            p.x = window.left + (window.right - window.left) / 2;
            p.y = window.top + (window.bottom - window.top) / 2;
            driver_set_cursor_position(p);
        }
    }
}

// Returns the previous node_t with the same tag_t as current
node_t get_previous_node()
{
    return (node_t)(g_tags[g_current_tag].current_window->prev);
}

// Returns the next node_t with the same tag_t as current
node_t get_next_node()
{
    tag_t *thistag = &g_tags[g_current_tag];

    if (thistag->current_window && thistag->current_window->next) {
        return (node_t)(thistag->current_window->next);
    } else {
        return thistag->nodes;
    }
}

// Returns the number of Nodes with the same tag_t as current
int count_nodes()
{
    node_t temp;
    node_t nodes = g_tags[g_current_tag].nodes;

    int i = 0;
    for (temp = nodes; temp; temp = temp->next) {
        i++;
    }

    return i - 1;
}

// Minimizes all the windows with the specified tag_t
void minimize_tag(unsigned short tag_t)
{
    node_t temp;
    for (temp=g_tags[tag_t].nodes; temp; temp = temp->next)
    driver_set_window_visible(temp->window, false);
}

// This does the actual tiling
void arrange_windows()
{
    int a, i, x, y, width, height;
    unsigned short masterarea_count;
    node_t nodes;
    node_t temp;

    a = count_nodes();
    if (a == -1) {
        return;
    }
    i = 0;

    nodes = g_tags[g_current_tag].nodes;
    masterarea_count = g_tags[g_current_tag].masterarea_count;

    for (temp = nodes; temp; temp = (node_t)temp->next) {
        driver_set_window_visible(temp->window, true);

        if (a == 0) { // I think this is universal to all tiling modes
            x = 0;
            y = 0;
            width = g_screen_width;
            height = g_screen_height;
        } else {
            switch (g_tags[g_current_tag].tilingMode)
            {
                default:
                case MODE_VERTICAL:
                {
                    if (i < masterarea_count) {
                        x = 0;
                        y = (g_screen_height / masterarea_count) * i;
                        width = (g_screen_width / 2) + g_margin;
                        height = (g_screen_height / masterarea_count);
                    } else {
                        x = (g_screen_width / 2) + g_margin;
                        y = (g_screen_height / ((a + 1) - masterarea_count)) * (a - i);
                        width = (g_screen_width / 2) - g_margin;
                        height = (g_screen_height / ((a + 1) - masterarea_count));
                    }
                }
                break;
                case MODE_HORIZONTAL:
                {
                    if (i < masterarea_count) {
                        // Main window
                        x = (g_screen_width / masterarea_count) * i;
                        y = 0;
                        width = (g_screen_width / masterarea_count);
                        height = (g_screen_height / 2) + g_margin;
                    } else {
                        // Normal windows to be tiled
                        x = (g_screen_width / ((a + 1) - masterarea_count)) * (a - i);
                        y = (g_screen_height / 2) + g_margin;
                        width = (g_screen_width / ((a + 1) - masterarea_count));
                        height = (g_screen_height / 2) - g_margin;
                    }
                }
                break;
                case MODE_GRID: // See dvtm-license.txt
                {
                    int ah, aw, rows, cols;
                    for (cols = 0; cols <= (a + 1)/2; cols++) {
                        if (cols * cols >= (a + 1)) {
                            break;
                        }
                    }
                    rows = (cols && (cols - 1) * cols >= (a + 1)) ? cols - 1 : cols;
                    height = g_screen_height / (rows ? rows : 1);
                    width = g_screen_width / (cols ? cols : 1);
                    if (rows > 1 && i == (rows * cols) - cols && ((a + 1) - i) <= ((a + 1) % cols)) {
                        width = g_screen_width / ((a + 1) - i);
                    }
                    x = (i % cols) * width;
                    y = (i / cols) * height;
                    ah = (i >= cols * (rows - 1)) ? g_screen_height - height * rows: 0;
                    if (rows > 1 && i == (a + 1) - 1 && ((a + 1) - i) < ((a + 1) % cols)) {
                        aw = g_screen_width - width * ((a + 1) % cols);
                    } else {
                        aw = ((i + 1) % cols == 0) ? g_screen_width - width * cols : 0;
                    }
                    width += aw;
                    height += ah;
                }
                break;
                case MODE_FULLSCREEN:
                x = 0;
                y = 0;
                width = g_screen_width;
                height = g_screen_height;
                break;
            }
        }

        //SetWindowPos(temp->window, HWND_TOP, x + g_screen_x, y + g_screen_y, width, height, SWP_SHOWWINDOW);

        rect_t r;
        r.left = x + g_screen_x;
        r.right = r.left + width;
        r.top = y + g_screen_y;
        r.bottom = r.top + height;

        driver_set_window_rect(temp->window, r);

        i++;
    }

    focus_current();
}

void toggle_tag(unsigned short tag_t) {
    window_t window = driver_get_foreground_window();

    if (find_node(window, tag_t)) {
        remove_node(window, tag_t);
        driver_set_window_visible(window, false);
    } else {
        add_node(window, tag_t);

        if (g_one_tag_per_window) {
            remove_node(window, g_current_tag);
            driver_set_window_visible(window, false);
        }
    }

    arrange_windows();
}

void register_hotkeys(window_t window)
{
    char key[2];
    int i;

    /*driver_hotkey_add(ACTION_SELECT_UP, g_modkeys, 'K');
    driver_hotkey_add(ACTION_SELECT_DOWN, g_modkeys, 'J');
    driver_hotkey_add(ACTION_MOVE_MAIN, g_modkeys, VK_RETURN);
    driver_hotkey_add(ACTION_EXIT, g_modkeys, VK_ESCAPE);
    driver_hotkey_add(ACTION_MARGIN_LEFT, g_modkeys, 'H');
    driver_hotkey_add(ACTION_MARGIN_RIGHT, g_modkeys, 'L');
    driver_hotkey_add(ACTION_IGNORE, g_modkeys, 'I');
    driver_hotkey_add(ACTION_MOUSE_LOCK, g_modkeys, 'U');
    driver_hotkey_add(ACTION_TILING_MODE, g_modkeys, VK_SPACE);
    driver_hotkey_add(ACTION_MOVE_UP, g_modkeys | MODIFIER_SHIFT, 'K');
    driver_hotkey_add(ACTION_MOVE_DOWN, g_modkeys | MODIFIER_SHIFT, 'J');
    driver_hotkey_add(ACTION_DISP_CLASS, g_modkeys, 'Y');
    driver_hotkey_add(ACTION_TILE, g_modkeys, 'O');
    driver_hotkey_add(ACTION_UNTILE, g_modkeys, 'P');
    driver_hotkey_add(ACTION_INC_AREA, g_modkeys, 'Z');
    driver_hotkey_add(ACTION_DEC_AREA, g_modkeys, 'X');
    driver_hotkey_add(ACTION_CLOSE_WIN, g_modkeys, 'C');*/

    // Tags
    for (i = 0; i < TAGS; i++) {
        sprintf(key, "%d", i + 1);
        //driver_hotkey_add(ACTION_SWITCH_T1 + i, g_modkeys, *key); // Switch to tag_t N
        //driver_hotkey_add(ACTION_TOGGLE_T1 + i, g_modkeys | MODIFIER_SHIFT, *key); // Toggle tag_t N
    }
}

void unregister_hotkeys(window_t window)
{
    for (int i = 1; i <= 27; i++) {
        //driver_hotkey_remove((hotkey_t)i);
    }
}

void update_mouse_pos(window_t window)
{
    pos_t cursor = driver_get_cursor_position();

    if ((cursor.x < g_screen_x) || (cursor.x > g_screen_x + g_screen_width) || (cursor.y < g_screen_y) || (cursor.y > g_screen_y + g_screen_height)) {
        if (g_mouse_pos_out == 0) {
            g_mouse_pos_out = 1;
            unregister_hotkeys(window);
        }
    } else {
        if (g_mouse_pos_out == 1) {
            g_mouse_pos_out = 0;
            //Sleep(500);
            register_hotkeys(window);
        }
    }
}

// Add desktop windows to the grid to be tiled
void enumerate_windows_add_proc(window_t w) {
    if (driver_should_tile_window_p(w)) {
        add_node(w, g_current_tag);
    }
}

void enumerate_windows_restore_proc(window_t w) {
    if (driver_should_tile_window_p(w)) {
        driver_set_window_visible(w, true);
    }
}

#ifdef DRIVER_WINDOWS
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
    // Process command line
    LPWSTR *argv = NULL;
    int argc;

    argv = CommandLineToArgvW(GetCommandLineW(), &argc);
#else
int main(int argc, char **argv) {
#endif

    int i;
    unsigned short tilingMode = DEFAULT_TILING_MODE;

    for (i = 0; i < argc; i++) {
        char arg[128];
        sprintf(arg, "%s", argv[i]);

        if (i < (argc - 1)) {
            char nextarg[128];
            sprintf(nextarg, "%s", argv[i + 1]);

            if (!strcmp(arg, "-o")) {
                g_alpha = atoi(nextarg);
            } else if (!strcmp(arg, "-i")) {
                if (g_ignore_count < MAX_IGNORE) {
                    sprintf(g_ignore_classes[g_ignore_count++], "%s", nextarg);
                }
            } else if (!strcmp(arg, "-a")) {
                g_include_mode = 1; // Include mode instead of exclude

                if (g_include_count < MAX_IGNORE) {
                    sprintf(g_include_classes[g_include_count++], "%s", nextarg);
                }
            } else if (!strcmp(arg, "-m")) {
                int y;
                g_modkeys = 0;

                for (y = 0; y < strlen(nextarg); y++) {
                    switch (nextarg[y])
                    {
                        case 'c':
                        g_modkeys |= MODIFIER_CTRL;
                        break;
                        case 'a':
                        g_modkeys |= MODIFIER_ALT;
                        break;
                        case 's':
                        g_modkeys |= MODIFIER_SHIFT;
                        break;
                        case 'w':
                        g_modkeys |= MODIFIER_SUPER;
                        break;
                    }
                }
            } else if (!strcmp(arg, "-t")) {
                tilingMode = atoi(nextarg);
            } else if (!strcmp(arg, "-left")) {
                g_screen_x = atoi(nextarg);
            } else if (!strcmp(arg, "-top")) {
                g_screen_y = atoi(nextarg);
            } else if (!strcmp(arg, "-width")) {
                g_screen_width = atoi(nextarg);
            } else if (!strcmp(arg, "-height")) {
                g_screen_height = atoi(nextarg);
            }
        }
        if (!strcmp(arg, "-v")) {
            driver_message("Version", VERSION);
            goto label_cleanup;
        } else if (!strcmp(arg, "-l")) {
            g_lock_mouse = 1;
        } else if (!strcmp(arg, "-x")) {
            g_experimental_mouse = 1;
        } else if (!strcmp(arg, "--one-tag_t")) {
            g_one_tag_per_window = 1;
        }
    }

    // Initialize g_tags
    for (i = 0; i < TAGS; i++) {
        g_tags[i].nodes = NULL;
        g_tags[i].last_node = NULL;
        g_tags[i].current_window = NULL;
        g_tags[i].tilingMode = tilingMode;
        g_tags[i].masterarea_count = 1;
    }

    // Screen options aren't being specified from the command line so set some defaults
    if (!g_screen_x && !g_screen_y && !g_screen_width && !g_screen_height) {
        rect_t workarea = driver_get_screen_rect(NULL);
        g_screen_x = workarea.left;
        g_screen_y = workarea.top;
        g_screen_width = workarea.right - workarea.left;
        g_screen_height = workarea.bottom - workarea.top;
    }

    //register_hotkeys(window);
    //update_mouse_pos(window);

    driver_enumerate_windows(enumerate_windows_restore_proc);
    driver_enumerate_windows(enumerate_windows_add_proc);

    arrange_windows();

    driver_main();

label_cleanup:
#ifdef DRIVER_WINDOWS
    LocalFree(argv);
#endif
    driver_destroy();
}
