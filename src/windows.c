// Windows defines and includes
#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0500
#include <windows.h>
#include <shellapi.h> // For CommandLineToArgvW

#define DRIVER_WINDOWS true

#include "types.h"
#include "driver.h"

// Shell hook stuff
UINT g_shell_hook_id; // Window Message id
BOOL (__stdcall *RegisterShellHookWindow_)(HWND) = NULL; // RegisterShellHookWindow function. For compatibillity we get it out of the dll though it is in the headers now

int32_t driver_get_screen_count() {
    return 1;
}

screen_t driver_get_screen(int32_t i) {
    return NULL;
}

rect_t driver_get_screen_rect(screen_t s) {
    RECT workarea;
    SystemParametersInfo(SPI_GETWORKAREA, 0, &workarea, 0);

    rect_r r;
    r.left = workarea.left;
    r.right = workarea.right;
    r.top = workarea.top;
    r.bottom = workarea.bottom;

    return r;
}

rect_t driver_get_window_rect(window_t w) {
    RECT _rect;
    GetWindowRect((HWND)w, &_rect);

    rect_t rect;
    rect.left   = _rect.left;
    rect.right  = _rect.right;
    rect.top    = _rect.top;
    rect.bottom = _rect.bottom;
}

void driver_set_window_rect(window_t w, rect_t r) {
    SetWindowPos((HWND)w, NULL, r.left, r.top, r.right - r.left, r.bottom - r.top, 0);
}

void driver_set_window_visible(window_t w, bool v) {
    WINDOWPLACEMENT placement = { sizeof(WINDOWPLACEMENT) };
    GetWindowPlacement((HWND)w, &placement);

    placement.showCmd = v ? SW_RESTORE : SW_SHOWMINIMIZED;

    SetWindowPlacement((HWND)w, &placement);
}

void driver_set_foreground_window(window_t w) {
    SetForegroundWindow((HWND)w);
}

string_t driver_get_window_name(window_t w) {
    LPSTR temp = (LPSTR)malloc(sizeof(TCHAR) * 128); // I don't like this, but it works

    GetWindowClass((HWND)w, temp, 128);

    return (string_t)temp;
}

pos_t driver_get_cursor_position() {
    POINT cursor;
    GetCursorPos(&cursor);

    pos_t p;
    p.x = cursor.x;
    p.y = cursor.y;

    return p;
}

void driver_set_cursor_position(pos_t p) {
    SetCursorPos(p.x, p.y);
}

void driver_set_cursor_rect(rect_t r) {
    RECT rect;
    rect.left   = r.left;
    rect.right  = r.right;
    rect.top    = r.top;
    rect.bottom = r.bottom;

    ClipCursor(&rect);
}

bool driver_should_tile_window_p(window_t w) {
    HWND window = (HWND)w;

    // Some criteria for windows to be tiled
    if (!disableNext && !mouse_pos_out && IsWindowVisible(window) && (GetParent(window) == 0)) {
        int exstyle = GetWindowLong(window, GWL_EXSTYLE);
        window_t owner = GetWindow(window, GW_OWNER);

        rect_t rect = driver_get_window_rect(window);

        // Check that window is within this screen.
        if (screen_width > 0 && screen_height > 0) {
            if (!((rect.left > screen_x || rect.right > screen_x) &&
            (rect.left < screen_x + screen_width || rect.right < screen_x + screen_width))) {
                return FALSE;
            }

            if (!((rect.top > screen_y || rect.bottom > screen_y) &&
            (rect.top < screen_y + screen_height || rect.bottom < screen_x + screen_height))) {
                return FALSE;
            }
        }

        if ((((exstyle & WS_EX_TOOLWINDOW) == 0) && (owner == 0)) || ((exstyle & WS_EX_APPWINDOW) && (owner != 0))) {
            int i;
            LPSTR temp = (LPSTR)malloc(sizeof(TCHAR) * 128);
            GetClassName(window, temp, 128);

            if (include_mode == 1) {
                for (i = 0; i < MAX_IGNORE; i++) {
                    if (!strcmp(temp, includeClasses[i])) { free(temp); return TRUE; }
                }

                free(temp);
                return FALSE;
            } else {
                for (i = 0; i < MAX_IGNORE; i++) {
                    if (!strcmp(temp, ignoreClasses[i])) { free(temp); return FALSE; }
                }
            }

            free(temp);
            return TRUE;
        }
    }
    return FALSE;
}

BOOL CALLBACK _driver_enumerate_windows_proc(HWND w, LPARAM lParam) {
    if (driver_should_tile_window_p((window_t)w)) {
        cb_enumerate_windows_t f = (cb_enumerate_windows_t)lParam;
        f((window_t)w);
    }

    return TRUE;
}

void driver_enumerate_windows(cb_enumerate_windows_t f) {
    EnumWindows(_driver_enumerate_windows_proc, (LPARAM)f);
}

void driver_hotkey_add(hotkey_t h, modkeys_t m, keys_t k) {
    RegisterHotkey(g_window, h, m, k);
}

void driver_hotkey_remove(hotkey_t h) {
    UnregisterHotkey(g_window, h);
}

void driver_message(string_t title, string_t msg) {
    MessageBox(NULL, msg, title, MB_OK);
}

LRESULT CALLBACK WndProc(window_t window, UINT msg, WPARAM wParam, LPARAM lParam)
{
    node_t *current = NULL;
    node_t *nodes;
    unsigned short tag;

    switch (msg)
    {
        case WM_CREATE:
        if (experimental_mouse) {
            SetTimer(window, TIMER_UPDATE_MOUSE, 500, NULL); // Poll for mouse position
        }
        break;

        case WM_CLOSE:
        {
            ClipCursor(0); // Release Cursor Lock
            DeregisterShellHookWindow(window);
            UnregisterHotkeys(window);
            if (experimental_mouse) {
                KillTimer(window, TIMER_UPDATE_MOUSE); // Mouse Poll Timer
            }
            for (tag=0; tag<TAGS; tag++) {
                nodes = tags[tag].nodes;
                for (current = nodes; current;) {
                    node_t *next = current->next;
                    driver_set_window_visible(current->window, true);
                    RemoveNode(current->window, tag);
                    current = next;
                }
                DestroyWindow(window);
            }
        }
        break;

        case WM_DESTROY:
        PostQuitMessage(WM_QUIT);
        break;

        case WM_HOTKEY:
        if (wParam >= KEY_TOGGLE_T1 && wParam < (KEY_TOGGLE_T1 + TAGS)) {
            ToggleTag(wParam - KEY_TOGGLE_T1);
            break;
        } else if (wParam >= KEY_SWITCH_T1 && wParam < (KEY_SWITCH_T1 + TAGS)) {
            MinimizeTag(current_tag);
            current_tag = wParam - KEY_SWITCH_T1;
            ArrangeWindows();
            break;
        }

        current = tags[current_tag].current_window;

        switch (wParam)
        {
            case KEY_SELECT_UP:
            if (current) {
                tags[current_tag].current_window = GetNextNode();
                FocusCurrent();
            }
            break;

            case KEY_SELECT_DOWN:
            if (current) {
                tags[current_tag].current_window = GetPreviousNode();
                FocusCurrent();
            }
            break;

            case KEY_MOVE_MAIN:
            SwapWindowWithNode(tags[current_tag].nodes);
            ArrangeWindows();
            break;

            case KEY_EXIT:
            PostMessage(window, WM_CLOSE, 0, 0);
            break;

            case KEY_MARGIN_LEFT:
            margin -= 20;
            ArrangeWindows();
            break;

            case KEY_MARGIN_RIGHT:
            margin += 20;
            ArrangeWindows();
            break;

            case KEY_IGNORE:
            if (!disableNext) {
                disableNext = 1;
            } else {
                disableNext = 0;
            }
            break;

            case KEY_MOUSE_LOCK:
            if (lockMouse) {
                lockMouse = 0;
                driver_set_cursor_rect(NULL);
            } else {
                lockMouse = 1;
                FocusCurrent();
            }
            break;

            case KEY_TILING_MODE:
            tags[current_tag].tilingMode = (tags[current_tag].tilingMode + 1) % MODE_END;
            ArrangeWindows();
            break;

            case KEY_MOVE_UP:
            if (current) {
                SwapWindowWithNode(GetNextNode());
                ArrangeWindows();
            }
            break;

            case KEY_MOVE_DOWN:
            if (current) {
                SwapWindowWithNode(GetPreviousNode());
                ArrangeWindows();
            }
            break;

            case KEY_DISP_CLASS:
            {
                LPSTR temp = (LPSTR)malloc(sizeof(TCHAR) * 128);
                GetClassName(driver_get_foreground_window(), temp, 128);
                driver_message("Window Class", temp);
                free(temp);
            }
            break;

            case KEY_TILE:
            if (driver_should_tile_window_p(driver_get_foreground_window())) {
                AddNode(driver_get_foreground_window(), current_tag);
                ArrangeWindows();
            }
            break;

            case KEY_UNTILE:
            FullRemoveNode(GetForegroundWindow());
            ArrangeWindows();
            break;

            case KEY_INC_AREA:
            tags[current_tag].masterarea_count++;
            ArrangeWindows();
            break;

            case KEY_DEC_AREA:
            tags[current_tag].masterarea_count--;
            ArrangeWindows();
            break;

            case KEY_CLOSE_WIN:
            PostMessage(GetForegroundWindow(), WM_CLOSE, 0, 0);
            break;
        }
        break;

        case WM_TIMER:
        switch (wParam)
        {
            case TIMER_UPDATE_MOUSE:
            UpdateMousePos(window);
            break;
        }
        break;

        default:
        if (msg == g_shell_hook_id) { // Handle the Shell Hook message
            switch (wParam)
            {
                case HSHELL_WINDOWCREATED:
                if (IsGoodWindow((window_t)lParam)) {
                    AddNode((window_t)lParam, current_tag);
                    ArrangeWindows();
                    FocusCurrent();
                }
                break;

                case HSHELL_WINDOWDESTROYED:
                FullRemoveNode((window_t)lParam);
                ArrangeWindows();
                FocusCurrent();
                break;

                case HSHELL_RUDEAPPACTIVATED:
                case HSHELL_WINDOWACTIVATED:
                {
                    node_t *found = FindNode((window_t)lParam, current_tag);
                    if (found) {
                        tags[current_tag].current_window = current = found;
                        FocusCurrent();
                    }
                }
                break;
            }
        } else {
            return DefWindowProc(window, msg, wParam, lParam);
        }
    }
    return 0;
}

bool driver_init() {
    WNDCLASSEX winClass;
    window_t window;
    MSG msg;

    LocalFree(argv);

    winClass.cbSize = sizeof(WNDCLASSEX);
    winClass.style = 0;
    winClass.lpfnWndProc = WndProc;
    winClass.cbClsExtra = 0;
    winClass.cbWndExtra = 0;
    winClass.hInstance = hInstance;
    winClass.hIcon = NULL;
    winClass.hIconSm = NULL;
    winClass.hCursor = NULL;
    winClass.hbrBackground = NULL;
    winClass.lpszMenuName = NULL;
    winClass.lpszClassName = NAME;

    if (!RegisterClassEx(&winClass)) {
        driver_message("Error", "Error Registering Window Class");
        return 0; // Bail
    }

    window = CreateWindowEx(0, NAME, NAME, 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, hInstance, NULL);

    if (!window) {
        driver_message("Error", "Error Creating Window");
        return 0; // Bail
    }

    // Get function pointer for RegisterShellHookWindow
    if ( RegisterShellHookWindow_ == NULL )
    {
        RegisterShellHookWindow_ = (BOOL (__stdcall *)(window_t))GetProcAddress(GetModuleHandle("USER32.DLL"), "RegisterShellHookWindow");
        if (RegisterShellHookWindow_ == NULL) {
            MessageBox(NULL, "Could not find RegisterShellHookWindow", "Error", MB_OK | MB_ICONERROR);
            return 0;
        }
    }

    RegisterShellHookWindow_(window);
    g_shell_hook_id = RegisterWindowMessage("SHELLHOOK"); // Grab a dynamic id for the SHELLHOOK message to be used later
}

void driver_destroy() {
    // Unhook shell
}

bool driver_main() {
    while (GetMessage(&msg, NULL, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return true;
}
