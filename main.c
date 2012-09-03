/*
 * vim: ts=8 noexpandtab sw=8 :
 *	HashTWM
 *	An automatic Tiling Window Manager for XP/Vista in spirit of dwm
 *	Copyright 2008-2009, Zane Ashby, http://demonastery.org
 */

/*
 * 	TODO
 * 	Clean up and get rid of unneccesary stuff to avoid feature creep
 * 	Add a popup window on tag change and for other statuses, shouldn't be too hard
 * 	Draw some sort of window outline to show activation
 * 	Investigate window activation code
 * 	Implement a window tagging system like dwm
 * 	Think about tiling mode DLL's or using a scripting language (like Lua^H^H^HLisp!) for adding tiling modes
 * 	Rethink using a Keyboard hook, emacs style maps could be neat
 */

#define NAME			"HashTWM" 	/* Used for Window Name/Class */
#define VERSION			"HashTWM 0.9Beta" 	/* Used for Version Box - Wait there isn't one, oh well */

//#define REMOTE // Not finished, but half working

/* Windows defines and includes */
#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT		0x0500
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <shellapi.h> 	/* For CommandLineToArgvW */

#define DEFAULT_MODKEY 		MOD_CONTROL | MOD_ALT
#define MAX_IGNORE 		16 	/* Allows 16 window classes to be ignored */
#define DEFAULT_TILING_MODE	0 	/* Vertical tiling is the default */
#define TAGS			9

/* Controls */
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
	KEY_SWITCH_T1=100,
	KEY_TOGGLE_T1=200
};

/* Node */
typedef struct
{
	HWND hwnd; /* Used as key */
	void* prev;
	void* next;
} node;

/* Tags */
typedef struct
{
	node* nodes;
	node* last_node;
	node* current_window;
	/* Tiling modes: 0=Vertical, 1=Horizontal, 2=Grid, 3=Fullscreen */
	unsigned short tilingMode;
	/* Xmonad style Master area count */
	unsigned short masterarea_count;
} tag;

/* Global Variables */
tag tags[TAGS];
unsigned short current_tag = 0;
int screen_x, screen_y, screen_width, screen_height;
unsigned short experimental_mouse = 0;
unsigned short mouse_pos_out = 0;
int margin = 120;
unsigned short disableNext = 0;
unsigned short lockMouse = 0;
unsigned short alpha = 255;
unsigned short borders = 1;
unsigned short ignoreCount = 0;
unsigned short ignoreCountBorders = 0;
int modkeys = DEFAULT_MODKEY;
char ignoreClasses[MAX_IGNORE][128];		/* Don't include these classes in tiling */
char ignoreClassesBorders[MAX_IGNORE][128]; 	/* Don't remove borders from these classes */

/* Shell Hook stuff */
typedef BOOL (*RegisterShellHookWindowProc) (HWND);
RegisterShellHookWindowProc RegisterShellHookWindow;
UINT shellhookid;	/* Window Message id */

#ifdef REMOTE
/* Remote Interface Stuff */
typedef struct
{
	unsigned short dest;	/* 0 (HashTWM) or 1 (clients) */
	int mode;
	int param;
} RemoteInterface;

void SendRemoteQuery(HWND selfhwnd, int mode, int param)
{
	RemoteInterface ri;
	COPYDATASTRUCT cds;

	ri.dest = 0; /* Temp - Broadcast to us */
	ri.mode = mode;
	ri.param = param;
	cds.dwData = 1;
	cds.cbData = sizeof(ri);
	cds.lpData = &ri;

	SendMessage(FindWindow("HashTWM", NULL), WM_COPYDATA, (WPARAM)(HWND)selfhwnd, (LPARAM)(LPVOID)&cds);
}
#endif

void RemoveTransparency(HWND hwnd)
{
	if (alpha < 255) {
		SetWindowLong(hwnd, GWL_EXSTYLE, GetWindowLong(hwnd, GWL_EXSTYLE) & ~WS_EX_LAYERED);
	}
}

void AddTransparency(HWND hwnd)
{
	if (alpha < 255) {
		SetWindowLong(hwnd, GWL_EXSTYLE, GetWindowLong(hwnd, GWL_EXSTYLE) | WS_EX_LAYERED);
		SetLayeredWindowAttributes(hwnd, RGB(255, 0, 0), alpha, LWA_COLORKEY | LWA_ALPHA);
	}
}

int IgnoreBorders(HWND hwnd) {
	LPSTR temp = (LPSTR)malloc(sizeof(TCHAR) * 128); 	/* I don't like this, but it works */
	GetClassName(hwnd, temp, 128);
	int i;
	for (i = 0; i < MAX_IGNORE; i++) {
		if (!strcmp(temp, ignoreClassesBorders[i])) { return TRUE; }
	}
	free(temp);
	return FALSE;
}

void RemoveBorder(HWND hwnd)
{
	if ((borders && !IgnoreBorders(hwnd)) || (!borders && IgnoreBorders(hwnd))) {
		SetWindowLong(hwnd, GWL_STYLE, (GetWindowLong(hwnd, GWL_STYLE) & ~(WS_CAPTION | WS_SIZEBOX)));
	}
}

void AddBorder(HWND hwnd)
{
	if ((borders && !IgnoreBorders(hwnd)) || (!borders && IgnoreBorders(hwnd))) {
		SetWindowLong(hwnd, GWL_STYLE, (GetWindowLong(hwnd, GWL_STYLE) | (WS_CAPTION | WS_SIZEBOX)));
		SetWindowPos(hwnd, 0, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER); 	/* Fix? */
	}
}

int IsGoodWindow(HWND hwnd)
{
	/* Some criteria for windows to be tiled */
	if (!disableNext && !mouse_pos_out && IsWindowVisible(hwnd) && (GetParent(hwnd) == 0)) {
		int exstyle = GetWindowLong(hwnd, GWL_EXSTYLE);
		HWND owner = GetWindow(hwnd, GW_OWNER);
		if ((((exstyle & WS_EX_TOOLWINDOW) == 0) && (owner == 0)) || ((exstyle & WS_EX_APPWINDOW) && (owner != 0))) {
			LPSTR temp = (LPSTR)malloc(sizeof(TCHAR) * 128); 	/* I don't like this, but it works */
			GetClassName(hwnd, temp, 128);
			int i;
			for (i = 0; i < MAX_IGNORE; i++) {
				if (!strcmp(temp, ignoreClasses[i])) { return FALSE; }
			}
			free(temp);
			return TRUE;
		}
	}
	return FALSE;
}

/* List Methods */

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
	RemoveBorder(hwnd);
	AddTransparency(hwnd);

	if (FindNode(hwnd, tag)) return;

	node *new = (node*)malloc(sizeof(node));
	new->hwnd = hwnd;
	new->prev = NULL;
	new->next = NULL;

	AddTransparency(hwnd);

	if (tags[tag].nodes == NULL) {
		new->prev = new;
		tags[tag].nodes = new;
		tags[tag].current_window = new;
		tags[tag].last_node = new;
	} else {
		tags[tag].last_node->next = new;
		new->prev = tags[tag].last_node;
		tags[tag].last_node = new;
		tags[tag].nodes->prev = new;
	}
}

void RemoveNode(HWND hwnd, unsigned short tag)
{
	node *temp;
	temp = FindNode(hwnd, tag);
	if (!temp) return;
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
	RemoveTransparency(temp->hwnd);
	if (!FullFindNode(hwnd))
		AddBorder(temp->hwnd);
	free(temp);
	return;
}

void FullRemoveNode(HWND hwnd)
{
	unsigned short tag;
	for (tag=0; tag<TAGS; tag++)
		RemoveNode(hwnd, tag);
}

void ToggleTag(unsigned short tag) {
	HWND hwnd = GetForegroundWindow();

	if (FindNode(hwnd, tag))
		RemoveNode(hwnd, tag);
	else
		AddNode(hwnd, tag);
}

void SwapWindowWithNode(node *window)
{

	if (tags[current_tag].current_window == window) return;
	if (tags[current_tag].current_window && window) {
		AddTransparency(window->hwnd);
		HWND temp = window->hwnd;
		window->hwnd = tags[current_tag].current_window->hwnd;
		tags[current_tag].current_window->hwnd = temp;
		tags[current_tag].current_window = window;
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
		RemoveTransparency(current->hwnd);
	}
}

/*
 * Returns the previous Node with the same tag as current
 */
node* GetPreviousNode()
{
	return tags[current_tag].current_window->prev;
}

/*
 * Returns the next Node with the same tag as current
 */
node* GetNextNode()
{
	tag *thistag = &tags[current_tag];

	if (thistag->current_window && thistag->current_window->next)
		return thistag->current_window->next;
	else
		return thistag->nodes;
}

/*
 * Returns the number of Nodes with the same tag as current
 */
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

/*
 * Minimizes all the windows with the specified tag
 */
void MinimizeTag(unsigned short tag)
{
	node *temp;
	for (temp=tags[tag].nodes; temp; temp = temp->next)
		ShowWindow(temp->hwnd, SW_MINIMIZE);
}

/*
 * This does the actual tiling
 */
void ArrangeWindows()
{
	int a, i, x, y, width, height;
	unsigned short masterarea_count;

	a = CountNodes();
	if (a == -1) return;
	i = 0;

	node *nodes;
	node *temp;
	nodes = tags[current_tag].nodes;
	masterarea_count = tags[current_tag].masterarea_count;
	for (temp = nodes; temp; temp = temp->next) {
		ShowWindow(temp->hwnd, SW_RESTORE);
		if (a == 0) { 	/* I think this is universal to all tiling modes */
			x = 0;
			y = 0;
			width = screen_width;
			height = screen_height;
		} else {
			switch (tags[current_tag].tilingMode)
			{
				default:
				case 0: 	/* Vertical */
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
				case 1: 	/* Horizontal */
					{
						if (i < masterarea_count) {
							/* Main window */
							x = (screen_width / masterarea_count) * i;
							y = 0;
							width = (screen_width / masterarea_count);
							height = (screen_height / 2) + margin;
						} else {
							/* Normal windows to be tiled */
							x = (screen_width / ((a + 1) - masterarea_count)) * (a - i);
							y = (screen_height / 2) + margin;
							width = (screen_width / ((a + 1) - masterarea_count));
							height = (screen_height / 2) - margin;
						}
					}
					break;
				case 2: 	/* Grid - See dvtm-license.txt */
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
				case 3: 	/* Fullscreen - This could probably be changed to work better */
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

void RegisterHotkeys(HWND hwnd)
{
	RegisterHotKey(hwnd, KEY_SELECT_UP, modkeys, 'K'); 		/* Select Up */
	RegisterHotKey(hwnd, KEY_SELECT_DOWN, modkeys, 'J'); 		/* Select Down */
	RegisterHotKey(hwnd, KEY_MOVE_MAIN, modkeys, VK_RETURN); 	/* Move Window in to Main Area */
	RegisterHotKey(hwnd, KEY_EXIT, modkeys, VK_ESCAPE); 	/* Exit */
	RegisterHotKey(hwnd, KEY_MARGIN_LEFT, modkeys, 'H'); 		/* Margin Left */
	RegisterHotKey(hwnd, KEY_MARGIN_RIGHT, modkeys, 'L'); 		/* Margin Right */
	RegisterHotKey(hwnd, KEY_IGNORE, modkeys, 'I'); 		/* Ignore Mode */
	RegisterHotKey(hwnd, KEY_MOUSE_LOCK, modkeys, 'U'); 		/* Mouse Lock Mode */
	RegisterHotKey(hwnd, KEY_TILING_MODE, modkeys, VK_SPACE); 	/* Switch Tiling Mode */
	RegisterHotKey(hwnd, KEY_MOVE_UP, modkeys | MOD_SHIFT, 'K'); 	/* Move Window Up */
	RegisterHotKey(hwnd, KEY_MOVE_DOWN, modkeys | MOD_SHIFT, 'J'); 	/* Move Window Down */
	RegisterHotKey(hwnd, KEY_DISP_CLASS, modkeys, 'Y'); 	/* Display Window Class */
	RegisterHotKey(hwnd, KEY_TILE, modkeys, 'O'); 	/* Tile Window */
	RegisterHotKey(hwnd, KEY_UNTILE, modkeys, 'P'); 	/* Untile Window */
	RegisterHotKey(hwnd, KEY_INC_AREA, modkeys, 'Z'); 	/* Increase Master Area */
	RegisterHotKey(hwnd, KEY_DEC_AREA, modkeys, 'X'); 	/* Decrease Master Area */
	RegisterHotKey(hwnd, KEY_CLOSE_WIN, modkeys, 'C'); 	/* Close Foreground Window */

	// Tags
	char key[2];
	int i;
	for (i=0; i<TAGS; i++) {
		sprintf(key, "%d", i+1);
		RegisterHotKey(hwnd, KEY_SWITCH_T1+i, modkeys, *key); 	/* Switch to tag N */
		RegisterHotKey(hwnd, KEY_TOGGLE_T1+i, modkeys | MOD_SHIFT, *key); 	/* Toggle tag N */
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
			SetTimer(hwnd, 1, 60000, NULL);				/* Trim memory once a minute */
			if (experimental_mouse) {
				SetTimer(hwnd, 2, 500, NULL); 			/* Poll for mouse position */
			}
			SetProcessWorkingSetSize(GetCurrentProcess(), -1, -1);	/* Trim memory now */
			break;
		case WM_CLOSE:
			{
				ClipCursor(0); 	/* Release Cursor Lock */
				DeregisterShellHookWindow(hwnd);
				UnregisterHotkeys(hwnd);
				KillTimer(hwnd, 1); 	/* Memory Trim Timer */
				if (experimental_mouse) {
					KillTimer(hwnd, 2); 	/* Mouse Poll Timer */
				}
				for (tag=0; tag<TAGS; tag++) {
					nodes = tags[tag].nodes;
					for (current = nodes; current;) { 	/* Add window borders, reset opacity and remove */
						node *temp = NULL;
						ShowWindow(current->hwnd, SW_RESTORE);
						AddBorder(current->hwnd);
						RemoveTransparency(current->hwnd);
						temp = current->next;
						free(current);
						current = temp;
					}
					DestroyWindow(hwnd);
				}
			}
			break;
		case WM_DESTROY:
			PostQuitMessage(WM_QUIT);
			break;
#ifdef REMOTE
		case WM_COPYDATA:
			{
				PCOPYDATASTRUCT pCDS = (PCOPYDATASTRUCT)lParam;
				if ((pCDS->dwData == 1) && ((RemoteInterface *)(pCDS->lpData))->dest == 0) {
					switch (((RemoteInterface *)(pCDS->lpData))->mode)
					{
						case 0:
							//MessageBox(NULL, "Select UP", "", MB_OK);
							break;
						case 1:
							//MessageBox(NULL, "Select DOWN", "", MB_OK);
							break;
						default:
							MessageBox(NULL, "Unrecognised Message", "", MB_OK);
							break;
					}
				}
			}
			break;
#endif
		case WM_HOTKEY:
#ifdef REMOTE
			SendRemoteQuery(hwnd, wParam, 0);
#endif
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
				case KEY_SELECT_UP: 	/* Select Up */
					if (current) {
						AddTransparency(current->hwnd);
						tags[current_tag].current_window = GetNextNode();
						FocusCurrent();
					}
					break;
				case KEY_SELECT_DOWN: 	/* Select Down */
					if (current) {
						AddTransparency(current->hwnd);
						tags[current_tag].current_window = GetPreviousNode();
						FocusCurrent();
					}
					break;
				case KEY_MOVE_MAIN: 	/* Move Window in to Main Area */
					SwapWindowWithNode(tags[current_tag].nodes);
					ArrangeWindows();
					break;
				case KEY_EXIT: 	/* Exit */
					PostMessage(hwnd, WM_CLOSE, 0, 0);
					break;
				case KEY_MARGIN_LEFT: 	/* Margin Left */
					margin -= 20;
					ArrangeWindows();
					break;
				case KEY_MARGIN_RIGHT: 	/* Margin Right */
					margin += 20;
					ArrangeWindows();
					break;
				case KEY_IGNORE: 	/* Ignore Mode */
					if (!disableNext) {
						disableNext = 1;
					} else {
						disableNext = 0;
					}
					break;
				case KEY_MOUSE_LOCK: 	/* Mouse Lock Mode */
					if (lockMouse) {
						lockMouse = 0;
						ClipCursor(0);
					} else {
						lockMouse = 1;
						FocusCurrent();
					}
					break;
				case KEY_TILING_MODE: 	/* Switch Tiling Mode */
					tags[current_tag].tilingMode = (tags[current_tag].tilingMode + 1) % 3;
					ArrangeWindows();
					break;
				case KEY_MOVE_UP: 	/* Move Window Up */
					if (current) {
						SwapWindowWithNode(GetNextNode());
						ArrangeWindows();
					}
					break;
				case KEY_MOVE_DOWN: 	/* Move Window Down */
					if (current) {
						SwapWindowWithNode(GetPreviousNode());
						ArrangeWindows();
					}
					break;
				case KEY_DISP_CLASS: 	/* Display Window Class */
					{
						LPSTR temp = (LPSTR)malloc(sizeof(TCHAR) * 128);
						GetClassName(GetForegroundWindow(), temp, 128);
						MessageBox(NULL, temp, "Window Class", MB_OK);
						free(temp);
					}
					break;
				case KEY_TILE: 	/* Tile Window */
					if (IsGoodWindow(GetForegroundWindow())) {
						AddNode(GetForegroundWindow(), current_tag);
						ArrangeWindows();
					}
					break;
				case KEY_UNTILE: 	/* Untile Window */
					FullRemoveNode(GetForegroundWindow());
					ArrangeWindows();
					break;
				case KEY_INC_AREA: 	/* Increase Master Area */
					tags[current_tag].masterarea_count++;
					ArrangeWindows();
					break;
				case KEY_DEC_AREA: 	/* Decrease Master Area */
					tags[current_tag].masterarea_count--;
					ArrangeWindows();
					break;
				case KEY_CLOSE_WIN:	/* Close Foreground Window */
					PostMessage(GetForegroundWindow(), WM_CLOSE, 0, 0);
					break;
			}
			break;
		case WM_TIMER: 		/* Free Memory */
			switch (wParam)
			{
				case 1:
					SetProcessWorkingSetSize(GetCurrentProcess(), -1, -1);
					break;
				case 2:
					UpdateMousePos(hwnd);
					break;
			}
			break;
		default:
			if (msg == shellhookid) {	/* Handle the Shell Hook message */
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
					case HSHELL_WINDOWACTIVATED:
						{
							node *found = FindNode((HWND)lParam, current_tag);
							if (found) {
								if (current) {
									AddTransparency(current->hwnd);
								}
								current = found;
								FocusCurrent();
							}
						}
						break;
				}
			} else {
				return DefWindowProc(hwnd, msg, wParam, lParam); /* We aren't handling this message so return DefWindowProc */
			}
	}
	return 0;
}

/*
 *	BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam)
 *
 *	Add desktop windows to the grid to be tiled
 */
BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam)
{
	if (IsGoodWindow(hwnd)) { AddNode(hwnd, current_tag); }
	return TRUE;
}

BOOL CALLBACK EnumWindowsRestore(HWND hwnd, LPARAM lParam)
{
	if (IsGoodWindow(hwnd)) { ShowWindow(hwnd, SW_RESTORE); }
	return TRUE;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
	WNDCLASSEX winClass;
	HWND hwnd;
	MSG msg;

	/* Process Command Line - OH GOD */
	LPWSTR *argv = NULL;
	int argc;
	int i;
	unsigned short tilingMode;

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
			} else if (!strcmp(arg, "-bi")) {
				if (ignoreCountBorders < MAX_IGNORE) {
					sprintf(ignoreClassesBorders[ignoreCountBorders++], "%s", nextarg);
				}
			} else if (!strcmp(arg, "-m")) {
				modkeys = 0;
				int y;
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
		} else if (!strcmp(arg, "-b")) {
			borders = 0;
		}
	}

	/* Initialize tags */
	for (i=0; i<TAGS; i++) {
		tags[i].nodes = NULL;
		tags[i].last_node = NULL;
		tags[i].current_window = NULL;
		tags[i].tilingMode = DEFAULT_TILING_MODE;
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
		return 0; /* Bail */
	}

	hwnd = CreateWindowEx(0, NAME, NAME, 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, hInstance, NULL);

	if (!hwnd) {
		MessageBox(NULL, "Error Creating Window", "Error", MB_OK | MB_ICONERROR);
		return 0; /* Bail */
	}

	if (!screen_x && !screen_y && !screen_width && !screen_height) { /* Screen options aren't being specified from the command line so set some defaults */
		RECT workarea;
		SystemParametersInfo(SPI_GETWORKAREA, 0, &workarea, 0);
		screen_x = workarea.left;
		screen_y = workarea.top;
		screen_width = workarea.right - workarea.left;
		screen_height = workarea.bottom - workarea.top;
	}

	RegisterHotkeys(hwnd);
	UpdateMousePos(hwnd);

	EnumWindows(EnumWindowsRestore, 0); /* Restore windows on startup so they get tiled */
	EnumWindows(EnumWindowsProc, 0);

	ArrangeWindows();

	/* Get function pointer for RegisterShellHookWindow */
	RegisterShellHookWindow = (RegisterShellHookWindowProc)GetProcAddress(GetModuleHandle("USER32.DLL"), "RegisterShellHookWindow");
	if (RegisterShellHookWindow == NULL) {
		MessageBox(NULL, "Could not find RegisterShellHookWindow", "Error", MB_OK | MB_ICONERROR);
		return 0;
	}
	RegisterShellHookWindow(hwnd);
	shellhookid = RegisterWindowMessage("SHELLHOOK"); /* Grab a dynamic id for the SHELLHOOK message to be used later */

	while (GetMessage(&msg, NULL, 0, 0) > 0)
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return msg.wParam;
}
