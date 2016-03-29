//
// vim: ts=2 expandtab sw=2
// HashTWM
// An automatic Tiling Window Manager for XP/Vista in spirit of dwm
// Copyright 2008-2013, Zane Ashby, http://www.zaneashby.com
//

#define NAME      "HashTWM"
#define VERSION   "HashTWM 1.0"

// Windows defines and includes
#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0500
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <shellapi.h> // For CommandLineToArgvW

#define DEFAULT_MODKEY        MOD_CONTROL | MOD_ALT
#define MAX_IGNORE            16
#define DEFAULT_TILING_MODE   MODE_VERTICAL
#define TAGS                  9

// Keyboard controls
enum controls {
  KEY_SELECT_UP = 1,
  KEY_SELECT_DOWN,
  KEY_MOVE_MAIN,
  KEY_EXIT,
  KEY_MARGIN_LEFT,
  KEY_MARGIN_RIGHT,
  KEY_IGNORE,
  KEY_MOUSE_LOCK,
  KEY_TILING_MODE,
  KEY_MOVE_UP,
  KEY_MOVE_DOWN,
  KEY_DISP_CLASS,
  KEY_TILE,
  KEY_UNTILE,
  KEY_INC_AREA,
  KEY_DEC_AREA,
  KEY_CLOSE_WIN,
  KEY_LEFT,
  KEY_RIGHT,
  KEY_SWITCH_T1 = 100,
  KEY_TOGGLE_T1 = 200
};

// Tiling modes
enum tiling_modes {
  MODE_VERTICAL = 0,
  MODE_HORIZONTAL,
  MODE_GRID,
  MODE_FULLSCREEN,
  // Keep this at the end if adding tiling modes
  MODE_END
};

// Timer modes
enum timer_modes {
  TIMER_UPDATE_MOUSE = 0
};

// Node, for linked-list
typedef struct
{
  HWND hwnd; // Used as key
  RECT rect; // Used for restoring window layout
  void* prev;
  void* next;
} node;

// Tags / Workspaces
typedef struct
{
  node* nodes; // List of nodes
  node* last_node;
  node* current_window;
  unsigned short tilingMode;
  // Xmonad style Master area count
  unsigned short masterarea_count;
} tag;

// Global variables
tag tags[TAGS];
unsigned short current_tag = 0;
int screen_x, screen_y, screen_width, screen_height;
unsigned short experimental_mouse = 0;
unsigned short mouse_pos_out = 0;
int margin = 120;
unsigned short disableNext = 0;
unsigned short lockMouse = 0;
unsigned short alpha = 255;
unsigned short ignoreCount = 0;
int modkeys = DEFAULT_MODKEY;
char ignoreClasses[MAX_IGNORE][128]; // Exclude tiling from the classes in here
char includeClasses[MAX_IGNORE][128]; // Only tile the classes in here
unsigned short includeCount = 0;
unsigned short include_mode = 0; // Exclude by default
unsigned short one_tag_per_window = 0; // If 1, remove current tag when adding a new one

// Shell hook stuff
UINT shellhookid; // Window Message id
BOOL (__stdcall *RegisterShellHookWindow_)(HWND) = NULL; // RegisterShellHookWindow function. For compatibillity we get it out of the dll though it is in the headers now

int IsInList(char **list, unsigned int length, HWND hwnd) {
  int i;
  LPSTR temp = (LPSTR)malloc(sizeof(TCHAR) * 128); // I don't like this, but it works
  GetClassName(hwnd, temp, 128);

  for (i = 0; i < length; i++) {
    if (!strcmp(temp, list[i])) { free(temp); return TRUE; }
  }

  free(temp);

  return FALSE;
}

int IsGoodWindow(HWND hwnd)
{
  // Some criteria for windows to be tiled
  if (!disableNext && !mouse_pos_out && IsWindowVisible(hwnd) && (GetParent(hwnd) == 0)) {
    int exstyle = GetWindowLong(hwnd, GWL_EXSTYLE);
    HWND owner = GetWindow(hwnd, GW_OWNER);

    RECT rect;
    GetWindowRect(hwnd, &rect);

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
      GetClassName(hwnd, temp, 128);

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


//
// Linked-list methods
//

node* FindNode(HWND hwnd, unsigned short tag)
{
  node *temp;
  node *nodes = tags[tag].nodes;

  for (temp = nodes; temp; temp = temp->next) {
    if (temp->hwnd == hwnd) {
      return temp;
    }
  }

  return NULL;
}

node* FullFindNode(HWND hwnd)
{
  unsigned short tag;
  node *found;

  for (tag=0; tag<TAGS; tag++) {
    found = FindNode(hwnd, tag);
    if (found) return found;
  }

  return NULL;
}

void AddNode(HWND hwnd, unsigned short tag)
{
  node *new_node;

  if (FindNode(hwnd, tag)) return;

  new_node = (node*)malloc(sizeof(node));
  new_node->hwnd = hwnd;
  new_node->prev = NULL;
  new_node->next = NULL;

  // Save window layout
  GetWindowRect(hwnd, &new_node->rect);

  if (tags[tag].nodes == NULL) {
    new_node->prev = new_node;
    tags[tag].nodes = new_node;
    tags[tag].current_window = new_node;
    tags[tag].last_node = new_node;
  } else {
    tags[tag].last_node->next = new_node;
    new_node->prev = tags[tag].last_node;
    tags[tag].last_node = new_node;
    tags[tag].nodes->prev = new_node;
  }
}

void RemoveNode(HWND hwnd, unsigned short tag)
{
  node *temp;
  temp = FindNode(hwnd, tag);

  if (!temp) return;

  // Restore window layout
  SetWindowPos(hwnd, NULL,
      temp->rect.left, // x
      temp->rect.top,  // y
      temp->rect.right - temp->rect.left, // width
      temp->rect.bottom - temp->rect.top, // height
      0);

  // Remove the only node
  if (tags[tag].nodes == tags[tag].last_node) {
    tags[tag].nodes = NULL;
    tags[tag].last_node = NULL;
    tags[tag].current_window = NULL;
    // Remove the first node
  } else if (temp == tags[tag].nodes) {
    tags[tag].nodes = temp->next;
    tags[tag].nodes->prev = tags[tag].last_node;
    // Remove the last node
  } else if (temp == tags[tag].last_node) {
    tags[tag].last_node = temp->prev;
    tags[tag].nodes->prev = temp->prev;
    tags[tag].last_node->next = NULL;
    // Remove any other node
  } else {
    ((node*)temp->prev)->next = temp->next;
    ((node*)temp->next)->prev = temp->prev;
  }

  if (tags[tag].current_window == temp)
    tags[tag].current_window = temp->prev;

  free(temp);

  return;
}

void FullRemoveNode(HWND hwnd)
{
  unsigned short tag;

  for (tag=0; tag<TAGS; tag++)
    RemoveNode(hwnd, tag);
}

node* FindNodeInChain(node* head, int idx)
{
    node* nd = head;
	int i = 0;

    if (!head) return NULL;

    for (; i < idx && nd; i++) {
        nd = (node*)nd->next;
    }

    return nd;
}

void SwapWindowWithNode(node *window)
{

  if (tags[current_tag].current_window == window) return;
  if (tags[current_tag].current_window && window) {
    HWND temp = window->hwnd;
    window->hwnd = tags[current_tag].current_window->hwnd;
    tags[current_tag].current_window->hwnd = temp;
    tags[current_tag].current_window = window;
  }
}

void SwapWindowWithFirstNonMasterWindow()
{
    node* head = tags[current_tag].nodes;
    node* current = tags[current_tag].current_window;
    int sub_node_idx = tags[current_tag].masterarea_count;

    if (current != head)
    {
        node* nd = FindNodeInChain(head, sub_node_idx);
        SwapWindowWithNode(nd);
    }
}

void FocusCurrent()
{
  node *current = tags[current_tag].current_window;

  if (current) {
    SetForegroundWindow(current->hwnd);

    if (lockMouse) {
      RECT window;
      GetWindowRect(current->hwnd, &window);
      ClipCursor(&window);
      SetCursorPos(window.left + (window.right - window.left) / 2, window.top + (window.bottom - window.top) / 2);
    }
  }
}

// Returns the previous Node with the same tag as current
node* GetPreviousNode()
{
  return (node*)(tags[current_tag].current_window->prev);
}

// Returns the next Node with the same tag as current
node* GetNextNode()
{
  tag *thistag = &tags[current_tag];

  if (thistag->current_window && thistag->current_window->next)
    return (node*)(thistag->current_window->next);
  else
    return thistag->nodes;
}

// Returns the number of Nodes with the same tag as current
int CountNodes()
{
  node *temp;
  node *nodes = tags[current_tag].nodes;

  int i = 0;
  for (temp = nodes; temp; temp = temp->next) {
    i++;
  }

  return i - 1;
}

void MinimizeWindow(HWND hwnd)
{
  WINDOWPLACEMENT placement = { sizeof(WINDOWPLACEMENT) };
  GetWindowPlacement(hwnd, &placement);
  placement.showCmd = SW_SHOWMINIMIZED;
  SetWindowPlacement(hwnd, &placement);
}

void RestoreWindow(HWND hwnd)
{
  WINDOWPLACEMENT placement = { sizeof(WINDOWPLACEMENT) };
  GetWindowPlacement(hwnd, &placement);
  placement.showCmd = SW_RESTORE;
  SetWindowPlacement(hwnd, &placement);
}

// Minimizes all the windows with the specified tag
void MinimizeTag(unsigned short tag)
{
  node *temp;
  for (temp=tags[tag].nodes; temp; temp = temp->next)
    MinimizeWindow(temp->hwnd);
}

// This does the actual tiling
void ArrangeWindows()
{
  int a, i, x, y, width, height;
  unsigned short masterarea_count;
  node *nodes;
  node *temp;

  a = CountNodes();
  if (a == -1) return;
  i = 0;

  nodes = tags[current_tag].nodes;
  masterarea_count = tags[current_tag].masterarea_count;

  for (temp = nodes; temp; temp = temp->next) {
    RestoreWindow(temp->hwnd);

    if (a == 0) { // I think this is universal to all tiling modes
      x = 0;
      y = 0;
      width = screen_width;
      height = screen_height;
    } else {
      switch (tags[current_tag].tilingMode)
      {
        default:
        case MODE_VERTICAL:
          {
            if (i < masterarea_count) {
              x = 0;
              y = (screen_height / masterarea_count) * i;
              width = (screen_width / 2) + margin;
              height = (screen_height / masterarea_count);
            } else {
              x = (screen_width / 2) + margin;
              y = (screen_height / ((a + 1) - masterarea_count)) * (a - i);
              width = (screen_width / 2) - margin;
              height = (screen_height / ((a + 1) - masterarea_count));
            }
          }
          break;
        case MODE_HORIZONTAL:
          {
            if (i < masterarea_count) {
              // Main window
              x = (screen_width / masterarea_count) * i;
              y = 0;
              width = (screen_width / masterarea_count);
              height = (screen_height / 2) + margin;
            } else {
              // Normal windows to be tiled

              x = (screen_width / ((a + 1) - masterarea_count)) * (a - i);
              y = (screen_height / 2) + margin;
              width = (screen_width / ((a + 1) - masterarea_count));
              height = (screen_height / 2) - margin;
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
            height = screen_height / (rows ? rows : 1);
            width = screen_width / (cols ? cols : 1);
            if (rows > 1 && i == (rows * cols) - cols && ((a + 1) - i) <= ((a + 1) % cols)) {
              width = screen_width / ((a + 1) - i);
            }
            x = (i % cols) * width;
            y = (i / cols) * height;
            ah = (i >= cols * (rows - 1)) ? screen_height - height * rows: 0;
            if (rows > 1 && i == (a + 1) - 1 && ((a + 1) - i) < ((a + 1) % cols)) {
              aw = screen_width - width * ((a + 1) % cols);
            } else {
              aw = ((i + 1) % cols == 0) ? screen_width - width * cols : 0;
            }
            width += aw;
            height += ah;
          }
          break;
        case MODE_FULLSCREEN:
          x = 0;
          y = 0;
          width = screen_width;
          height = screen_height;
          break;
      }
    }

    SetWindowPos(temp->hwnd, HWND_TOP, x + screen_x, y + screen_y, width, height, SWP_SHOWWINDOW);
    i++;
  }

  FocusCurrent();
}

void ToggleTag(unsigned short tag) {
  HWND hwnd = GetForegroundWindow();

  if (FindNode(hwnd, tag)) {
    RemoveNode(hwnd, tag);
    MinimizeWindow(hwnd);
  } else {
    AddNode(hwnd, tag);

    if (one_tag_per_window) {
      RemoveNode(hwnd, current_tag);
      MinimizeWindow(hwnd);
    }
  }

  ArrangeWindows();
}

void RegisterHotkeys(HWND hwnd)
{
  char key[2];
  int i;

  RegisterHotKey(hwnd, KEY_SELECT_UP, modkeys, 'K');
  RegisterHotKey(hwnd, KEY_SELECT_DOWN, modkeys, 'J');
  RegisterHotKey(hwnd, KEY_MOVE_MAIN, modkeys, VK_RETURN);
  RegisterHotKey(hwnd, KEY_EXIT, modkeys, VK_ESCAPE);
  RegisterHotKey(hwnd, KEY_MARGIN_LEFT, modkeys, 'H');
  RegisterHotKey(hwnd, KEY_MARGIN_RIGHT, modkeys, 'L');
  RegisterHotKey(hwnd, KEY_IGNORE, modkeys, 'I');
  RegisterHotKey(hwnd, KEY_MOUSE_LOCK, modkeys, 'U');
  RegisterHotKey(hwnd, KEY_TILING_MODE, modkeys, VK_SPACE);
  RegisterHotKey(hwnd, KEY_MOVE_UP, modkeys | MOD_SHIFT, 'K');
  RegisterHotKey(hwnd, KEY_MOVE_DOWN, modkeys | MOD_SHIFT, 'J');
  RegisterHotKey(hwnd, KEY_DISP_CLASS, modkeys, 'Y');
  RegisterHotKey(hwnd, KEY_TILE, modkeys, 'O');
  RegisterHotKey(hwnd, KEY_UNTILE, modkeys, 'P');
  RegisterHotKey(hwnd, KEY_INC_AREA, modkeys, 'Z');
  RegisterHotKey(hwnd, KEY_DEC_AREA, modkeys, 'X');
  RegisterHotKey(hwnd, KEY_CLOSE_WIN, modkeys, 'C');
  RegisterHotKey(hwnd, KEY_LEFT, modkeys, VK_LEFT);
  RegisterHotKey(hwnd, KEY_RIGHT, modkeys, VK_RIGHT);

  // Tags
  for (i = 0; i < TAGS; i++) {
    sprintf(key, "%d", i + 1);
    RegisterHotKey(hwnd, KEY_SWITCH_T1 + i, modkeys, *key); // Switch to tag N
    RegisterHotKey(hwnd, KEY_TOGGLE_T1 + i, modkeys | MOD_SHIFT, *key); // Toggle tag N
  }
}

void UnregisterHotkeys(HWND hwnd)
{
  int i;
  for (i = 1; i <= 27; i++) UnregisterHotKey(hwnd, i);
}

void UpdateMousePos(HWND hwnd)
{
  POINT cursor;
  GetCursorPos(&cursor);

  if ((cursor.x < screen_x) || (cursor.x > screen_x + screen_width) || (cursor.y < screen_y) || (cursor.y > screen_y + screen_height)) {
    if (mouse_pos_out == 0) {
      mouse_pos_out = 1;
      UnregisterHotkeys(hwnd);
    }
  } else {
    if (mouse_pos_out == 1) {
      mouse_pos_out = 0;
      Sleep(500);
      RegisterHotkeys(hwnd);
    }
  }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  node *current = NULL;
  node *nodes;
  unsigned short tag;

  switch (msg)
  {
    case WM_CREATE:
      if (experimental_mouse) {
        SetTimer(hwnd, TIMER_UPDATE_MOUSE, 500, NULL); // Poll for mouse position
      }
      break;

    case WM_CLOSE:
      {
        ClipCursor(0); // Release Cursor Lock
        DeregisterShellHookWindow(hwnd);
        UnregisterHotkeys(hwnd);
        if (experimental_mouse) {
          KillTimer(hwnd, TIMER_UPDATE_MOUSE); // Mouse Poll Timer
        }
        for (tag=0; tag<TAGS; tag++) {
          nodes = tags[tag].nodes;
          for (current = nodes; current;) {
            node *next = current->next;
            RestoreWindow(current->hwnd);
            RemoveNode(current->hwnd, tag);
            current = next;
          }
          DestroyWindow(hwnd);
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
          PostMessage(hwnd, WM_CLOSE, 0, 0);
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
            ClipCursor(0);
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
            GetClassName(GetForegroundWindow(), temp, 128);
            MessageBox(NULL, temp, "Window Class", MB_OK);
            free(temp);
          }
          break;

        case KEY_TILE:
          if (IsGoodWindow(GetForegroundWindow())) {
            AddNode(GetForegroundWindow(), current_tag);
            ArrangeWindows();
          }
          break;

        case KEY_UNTILE:
          FullRemoveNode(GetForegroundWindow());
          ArrangeWindows();
          break;

        case KEY_INC_AREA:
          SwapWindowWithFirstNonMasterWindow();
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

        case KEY_LEFT:
          if(current_tag>0) {
            MinimizeTag(current_tag);
            current_tag--;
            ArrangeWindows();
          }
          break;

        case KEY_RIGHT:
          if(current_tag<9) {
            MinimizeTag(current_tag);
            current_tag++;
            ArrangeWindows();
          }
          break;
      }
      break;

    case WM_TIMER:
      switch (wParam)
      {
        case TIMER_UPDATE_MOUSE:
          UpdateMousePos(hwnd);
          break;
      }
      break;

    default:
      if (msg == shellhookid) { // Handle the Shell Hook message
        switch (wParam)
        {
          case HSHELL_WINDOWCREATED:
            if (IsGoodWindow((HWND)lParam)) {
              AddNode((HWND)lParam, current_tag);
              ArrangeWindows();
              FocusCurrent();
            }
            break;

          case HSHELL_WINDOWDESTROYED:
            FullRemoveNode((HWND)lParam);
            ArrangeWindows();
            FocusCurrent();
            break;

          case HSHELL_RUDEAPPACTIVATED:
          case HSHELL_WINDOWACTIVATED:
            {
              node *found = FindNode((HWND)lParam, current_tag);
              if (found) {
                tags[current_tag].current_window = current = found;
                FocusCurrent();
              }
            }
            break;
        }
      } else {
        return DefWindowProc(hwnd, msg, wParam, lParam);
      }
  }
  return 0;
}

// Add desktop windows to the grid to be tiled
BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam)
{
  if (IsGoodWindow(hwnd)) { AddNode(hwnd, current_tag); }
  return TRUE;
}

BOOL CALLBACK EnumWindowsRestore(HWND hwnd, LPARAM lParam)
{
  if (IsGoodWindow(hwnd)) { RestoreWindow(hwnd); }
  return TRUE;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
  WNDCLASSEX winClass;
  HWND hwnd;
  MSG msg;

  // Process command line
  LPWSTR *argv = NULL;
  int argc;
  int i;
  unsigned short tilingMode = DEFAULT_TILING_MODE;

  argv = CommandLineToArgvW(GetCommandLineW(), &argc);

  for (i = 0; i < argc; i++) {
    char arg[128];
    wsprintfA(arg, "%S", argv[i]);

    if (i < (argc - 1)) {
      char nextarg[128];
      wsprintfA(nextarg, "%S", argv[i + 1]);

      if (!strcmp(arg, "-o")) {
        alpha = atoi(nextarg);
      } else if (!strcmp(arg, "-i")) {
        if (ignoreCount < MAX_IGNORE) {
          sprintf(ignoreClasses[ignoreCount++], "%s", nextarg);
        }
      } else if (!strcmp(arg, "-a")) {
        include_mode = 1; // Include mode instead of exclude

        if (includeCount < MAX_IGNORE) {
          sprintf(includeClasses[includeCount++], "%s", nextarg);
        }
      } else if (!strcmp(arg, "-m")) {
        int y;
        modkeys = 0;

        for (y = 0; y < strlen(nextarg); y++) {
          switch (nextarg[y])
          {
            case 'c':
              modkeys |= MOD_CONTROL;
              break;
            case 'a':
              modkeys |= MOD_ALT;
              break;
            case 's':
              modkeys |= MOD_SHIFT;
              break;
            case 'w':
              modkeys |= MOD_WIN;
              break;
          }
        }
      } else if (!strcmp(arg, "-t")) {
        tilingMode = atoi(nextarg);
      } else if (!strcmp(arg, "-left")) {
        screen_x = atoi(nextarg);
      } else if (!strcmp(arg, "-top")) {
        screen_y = atoi(nextarg);
      } else if (!strcmp(arg, "-width")) {
        screen_width = atoi(nextarg);
      } else if (!strcmp(arg, "-height")) {
        screen_height = atoi(nextarg);
      }
    }
    if (!strcmp(arg, "-v")) {
      MessageBox(NULL, VERSION, "Version", MB_OK);
      LocalFree(argv);
      return 1;
    } else if (!strcmp(arg, "-l")) {
      lockMouse = 1;
    } else if (!strcmp(arg, "-x")) {
      experimental_mouse = 1;
    } else if (!strcmp(arg, "--one-tag")) {
      one_tag_per_window = 1;
    }
  }

  // Initialize tags
  for (i = 0; i < TAGS; i++) {
    tags[i].nodes = NULL;
    tags[i].last_node = NULL;
    tags[i].current_window = NULL;
    tags[i].tilingMode = tilingMode;
    tags[i].masterarea_count = 1;
  }

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
    MessageBox(NULL, "Error Registering Window Class", "Error", MB_OK | MB_ICONERROR);
    return 0; // Bail
  }

  hwnd = CreateWindowEx(0, NAME, NAME, 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, hInstance, NULL);

  if (!hwnd) {
    MessageBox(NULL, "Error Creating Window", "Error", MB_OK | MB_ICONERROR);
    return 0; // Bail
  }

  if (!screen_x && !screen_y && !screen_width && !screen_height) { // Screen options aren't being specified from the command line so set some defaults
    RECT workarea;
    SystemParametersInfo(SPI_GETWORKAREA, 0, &workarea, 0);
    screen_x = workarea.left;
    screen_y = workarea.top;
    screen_width = workarea.right - workarea.left;
    screen_height = workarea.bottom - workarea.top;
  }

  RegisterHotkeys(hwnd);
  UpdateMousePos(hwnd);

  EnumWindows(EnumWindowsRestore, 0); // Restore windows on startup so they get tiled
  EnumWindows(EnumWindowsProc, 0);

  ArrangeWindows();

  // Get function pointer for RegisterShellHookWindow
  if ( RegisterShellHookWindow_ == NULL )
  {
    RegisterShellHookWindow_ = (BOOL (__stdcall *)(HWND))GetProcAddress(GetModuleHandle("USER32.DLL"), "RegisterShellHookWindow");
    if (RegisterShellHookWindow_ == NULL) {
      MessageBox(NULL, "Could not find RegisterShellHookWindow", "Error", MB_OK | MB_ICONERROR);
      return 0;
    }
  }

  RegisterShellHookWindow_(hwnd);
  shellhookid = RegisterWindowMessage("SHELLHOOK"); // Grab a dynamic id for the SHELLHOOK message to be used later

  while (GetMessage(&msg, NULL, 0, 0) > 0)
  {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  return msg.wParam;
}
