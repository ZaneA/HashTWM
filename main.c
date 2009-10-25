/*
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
#define VERSION			"HashTWM 0.8Beta" 	/* Used for Version Box - Wait there isn't one, oh well */

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

/* Tags */
#define TAG_1 1
#define TAG_2 2
#define TAG_3 4
#define TAG_4 8
#define TAG_5 16
unsigned short current_tags = TAG_1;

/* Global Variables */
int screen_x, screen_y, screen_width, screen_height;
unsigned short experimental_mouse = 0;
unsigned short mouse_pos_out = 0;
int margin = 120;
unsigned short disableNext = 0;
unsigned short lockMouse = 0;
unsigned short tilingMode = 0; 	/* 0=Vertical, 1=Horizontal, 2=Grid, 3=Fullscreen */
unsigned short alpha = 255;
unsigned short borders = 1;
unsigned short ignoreCount = 0;
unsigned short ignoreCountBorders = 0;
unsigned short masterarea_count = 1; /* Xmonad style Master area count */
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

/* Node */
typedef struct
{
	HWND hwnd; /* Used as key */
	unsigned short tags; /* Bitmask - Actually not yet, but will become one */
	void* prev;
	void* next;
} node;

node *nodes = NULL; 	/* Should always point to first node */
node *current = NULL; 	/* Should always point to current node */

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

void AddNode(HWND hwnd)
{
	RemoveBorder(hwnd);
	AddTransparency(hwnd);

	node *new = (node*)malloc(sizeof(node));
	new->hwnd = hwnd;
	new->tags = current_tags;
	new->prev = NULL;
	new->next = NULL;

	AddTransparency(hwnd);

	if (nodes == NULL) {
		new->prev = new;
		nodes = new;
		current = nodes;
		return;
	}

	new->prev = nodes->prev;
	nodes->prev = new;
	new->next = nodes;
	nodes = new;
	current = nodes;
}

void RemoveNode(HWND hwnd)
{
	node *temp = nodes;
	for (temp = nodes; temp; temp = temp->next) {
		if (temp->hwnd == hwnd) {
			if (temp != nodes) {
				((node*)temp->prev)->next = temp->next;
			} else {
				nodes = temp->next;
			}
			if (temp->next) {
				((node*)temp->next)->prev = temp->prev;
			} else if (nodes) {
				nodes->prev = temp->prev;
			}
			node *temp2 = temp->prev;
			RemoveTransparency(temp->hwnd);
			AddBorder(temp->hwnd);
			free(temp);
			temp = temp2;
			current = NULL;
			return;
		}
	}
}

node* FindNode(HWND hwnd)
{
	node *temp = nodes;
	for (temp = nodes; temp; temp = temp->next) {
		if (temp->hwnd == hwnd) {
			return temp;
		}
	}
	return NULL;
}

void SwapWindowWithNode(node *window)
{
	if (current && window) {
		AddTransparency(window->hwnd);
		HWND temp = window->hwnd;
		window->hwnd = current->hwnd;
		current->hwnd = temp;
		current = window;
	}
}

void FocusCurrent()
{
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

node* GetFirstNode()
{
	node *temp, *first = nodes;
	for (temp = nodes; temp; temp = temp->next)
	{
		if (temp->tags == current_tags) {
			return temp;
		}
		if (temp == first) return current;
	}
}

/*
 * Returns the previous Node with the same tag as current
 */
node* GetPreviousNode()
{
	node *temp, *first = current;
	for (temp = current->prev; temp; temp = temp->prev) {
		if (temp->tags == current_tags) {
			return temp;
		}
		if (temp == first) return temp;
	}
}

/*
 * Returns the next Node with the same tag as current
 */
node* GetNextNode()
{
	node *temp, *first = current;
	for (temp = current; temp;) {
		if (temp->next) { temp = temp->next; } else { temp = nodes; }
		if (temp->tags == current_tags) {
			return temp;
		}
		if (temp == first) return temp;
	}
}

/*
 * Returns the number of Nodes with the same tag as current
 */
int CountNodes()
{
	node *temp = nodes;
	int i = 0;
	for (temp = nodes; temp; temp = temp->next) {
		if (temp->tags == current_tags)
			i++;
	}
	return i - 1;
}

/*
 * This does the actual tiling
 */
void ArrangeWindows()
{
	int a, i, x, y, width, height;

	a = CountNodes();
	if (a == -1) return;
	i = 0;

	node *temp = nodes;
//	for (temp = nodes; i <= a; temp = temp->next, i++) {
	for (temp = nodes; temp; temp = temp->next) {
		if (temp->tags == current_tags) {
			ShowWindow(temp->hwnd, SW_RESTORE);
			if (a == 0) { 	/* I think this is universal to all tiling modes */
				x = 0;
				y = 0;
				width = screen_width;
				height = screen_height;
			} else {
				switch (tilingMode)
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
		} else {
			ShowWindow(temp->hwnd, SW_MINIMIZE);
		}
	}
	FocusCurrent();
}

void RegisterHotkeys(HWND hwnd)
{
	RegisterHotKey(hwnd, 1, modkeys, 'K'); 		/* Select Up */
	RegisterHotKey(hwnd, 2, modkeys, 'J'); 		/* Select Down */
	RegisterHotKey(hwnd, 3, modkeys, VK_RETURN); 	/* Move Window in to Main Area */
	RegisterHotKey(hwnd, 4, modkeys, VK_ESCAPE); 	/* Exit */
	RegisterHotKey(hwnd, 5, modkeys, 'H'); 		/* Margin Left */
	RegisterHotKey(hwnd, 6, modkeys, 'L'); 		/* Margin Right */
	RegisterHotKey(hwnd, 7, modkeys, 'I'); 		/* Ignore Mode */
	RegisterHotKey(hwnd, 8, modkeys, 'U'); 		/* Mouse Lock Mode */
	RegisterHotKey(hwnd, 9, modkeys, VK_SPACE); 	/* Switch Tiling Mode */
	RegisterHotKey(hwnd, 10, modkeys | MOD_SHIFT, 'K'); 	/* Move Window Up */
	RegisterHotKey(hwnd, 11, modkeys | MOD_SHIFT, 'J'); 	/* Move Window Down */
	RegisterHotKey(hwnd, 12, modkeys, 'Y'); 	/* Display Window Class */
	RegisterHotKey(hwnd, 13, modkeys, 'O'); 	/* Tile Window */
	RegisterHotKey(hwnd, 14, modkeys, 'P'); 	/* Untile Window */
	RegisterHotKey(hwnd, 15, modkeys, 'Z'); 	/* Increase Master Area */
	RegisterHotKey(hwnd, 16, modkeys, 'X'); 	/* Decrease Master Area */
	RegisterHotKey(hwnd, 17, modkeys, 'C'); 	/* Close Foreground Window */

	// Tags
	RegisterHotKey(hwnd, 18, modkeys, '1'); 	/* Switch to tag 1 */
	RegisterHotKey(hwnd, 19, modkeys, '2'); 	/* Switch to tag 2 */
	RegisterHotKey(hwnd, 20, modkeys, '3'); 	/* Switch to tag 3 */
	RegisterHotKey(hwnd, 21, modkeys, '4'); 	/* Switch to tag 4 */
	RegisterHotKey(hwnd, 22, modkeys, '5'); 	/* Switch to tag 5 */

	RegisterHotKey(hwnd, 23, modkeys | MOD_SHIFT, '1'); 	/* Move to tag 1 */
	RegisterHotKey(hwnd, 24, modkeys | MOD_SHIFT, '2'); 	/* Move to tag 2 */
	RegisterHotKey(hwnd, 25, modkeys | MOD_SHIFT, '3'); 	/* Move to tag 3 */
	RegisterHotKey(hwnd, 26, modkeys | MOD_SHIFT, '4'); 	/* Move to tag 4 */
	RegisterHotKey(hwnd, 27, modkeys | MOD_SHIFT, '5'); 	/* Move to tag 5 */
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
			switch (wParam)
			{
				case 1: 	/* Select Up */
					if (current) {
						AddTransparency(current->hwnd);
						current = GetNextNode();
						FocusCurrent();
					}
					break;
				case 2: 	/* Select Down */
					if (current) {
						AddTransparency(current->hwnd);
						current = GetPreviousNode();
						FocusCurrent();
					}
					break;
				case 3: 	/* Move Window in to Main Area */
					SwapWindowWithNode(GetFirstNode());
					ArrangeWindows();
					break;
				case 4: 	/* Exit */
					PostMessage(hwnd, WM_CLOSE, 0, 0);
					break;
				case 5: 	/* Margin Left */
					margin -= 20;
					ArrangeWindows();
					break;
				case 6: 	/* Margin Right */
					margin += 20;
					ArrangeWindows();
					break;
				case 7: 	/* Ignore Mode */
					if (!disableNext) {
						disableNext = 1;
					} else {
						disableNext = 0;
					}
					break;
				case 8: 	/* Mouse Lock Mode */
					if (lockMouse) {
						lockMouse = 0;
						ClipCursor(0);
					} else {
						lockMouse = 1;
						FocusCurrent();
					}
					break;
				case 9: 	/* Switch Tiling Mode */
					tilingMode++;
					if (tilingMode > 3) {
						tilingMode = 0;
					}
					ArrangeWindows();
					break;
				case 10: 	/* Move Window Up */
					if (current) {
						SwapWindowWithNode(GetNextNode());
						ArrangeWindows();
					}
					break;
				case 11: 	/* Move Window Down */
					if (current) {
						SwapWindowWithNode(GetPreviousNode());
						ArrangeWindows();
					}
					break;
				case 12: 	/* Display Window Class */
					{
						LPSTR temp = (LPSTR)malloc(sizeof(TCHAR) * 128);
						GetClassName(GetForegroundWindow(), temp, 128);
						MessageBox(NULL, temp, "Window Class", MB_OK);
						free(temp);
					}
					break;
				case 13: 	/* Tile Window */
					if (IsGoodWindow(GetForegroundWindow())) {
						AddNode(GetForegroundWindow());
						ArrangeWindows();
					}
					break;
				case 14: 	/* Untile Window */
					RemoveNode(GetForegroundWindow());
					ArrangeWindows();
					break;
				case 15: 	/* Increase Master Area */
					masterarea_count++;
					ArrangeWindows();
					break;
				case 16: 	/* Decrease Master Area */
					masterarea_count--;
					ArrangeWindows();
					break;
				case 17:	/* Close Foreground Window */
					PostMessage(GetForegroundWindow(), WM_CLOSE, 0, 0);
					break;
				case 18:	/* Switch to tag 1 */
					current_tags = TAG_1;
					ArrangeWindows();
					break;
				case 19:	/* Switch to tag 2 */
					current_tags = TAG_2;
					ArrangeWindows();
					break;
				case 20:	/* Switch to tag 3 */
					current_tags = TAG_3;
					ArrangeWindows();
					break;
				case 21:	/* Switch to tag 4 */
					current_tags = TAG_4;
					ArrangeWindows();
					break;
				case 22:	/* Switch to tag 5 */
					current_tags = TAG_5;
					ArrangeWindows();
					break;
				case 23:	/* Move to tag 1 */
					current->tags = TAG_1;
					ArrangeWindows();
					break;
				case 24:	/* Move to tag 2 */
					current->tags = TAG_2;
					ArrangeWindows();
					break;
				case 25:	/* Move to tag 3 */
					current->tags = TAG_3;
					ArrangeWindows();
					break;
				case 26:	/* Move to tag 4 */
					current->tags = TAG_4;
					ArrangeWindows();
					break;
				case 27:	/* Move to tag 5 */
					current->tags = TAG_5;
					ArrangeWindows();
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
							AddNode((HWND)lParam);
							ArrangeWindows();
							FocusCurrent();
						}
						break;
					case HSHELL_WINDOWDESTROYED:
						RemoveNode((HWND)lParam);
						ArrangeWindows();
						FocusCurrent();
						break;
					case HSHELL_WINDOWACTIVATED:
						{
							node *found = FindNode((HWND)lParam);
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
	if (IsGoodWindow(hwnd)) { AddNode(hwnd); }
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
