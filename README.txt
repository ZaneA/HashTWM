HashTWM 0.8 Beta (Somewhat functional)
A dwm-like automatic tiling window manager for Microsoft Windows, download at http://hashtwm.demonastery.org

Usage:
Run HashTWM.exe.
All windows on screen will automatically be tiled in a vertical layout, similar to dwm in the Linux world.
You should be able to combine a Virtual Desktop manager with multiple instances of HashTWM (with different mod keys) for workspace like support.
Beware this is alpha software, so your milage may vary.

Key bindings (By default mod is Ctrl + Alt):
Mod + Escape - Exit (Windows should be restored to normal)
Mod + j/k - Cycle through tiled windows
Mod + Shift + j/k - Move selected window down/up
Mod + Enter - Switch selected window in to main area
Mod + h/l - Shift split offset
Mod + z/x - Increase/Decrease number of windows in main area
Mod + c - Close foreground window
Mod + Space - Switch between tiling modes, Vertical/Horizontal stack, Grid, and Fullscreen
Mod + y - Display foreground window class (For use with -i parameter)
Mod + u - Toggle lock cursor mode. In this mode cursor is locked inside the active window
Mod + i - Toggle ignore mode. If ignore is on then new windows will not be tiled but will appear as normal
Mod + o - Force Tile Foreground window
Mod + p - Force Untile Foreground window (useful in combination with above for moving windows to another monitor)

Mod + 1-5 - Switch to tag
Mod + Shift + 1-5 - Move Window to tag


Command Line Options:
-v		Display Version and exit
-l		Enable mouse lock by default
-o [opacity]	Set non active windows to [opacity], 0 being fully transparent and 255 being fully opaque
-i [class]	Add window class [class] to ignore list
-bi [class]	Add window class [class] to border removal ignore list
-m [mod]	Change the modifier key, eg. -m ws will set modifier to Win + Shift, you can combine the following:
		c - Control
		a - Alt
		s - Shift
		w - Win
-t [mode]	Change the default tiling mode, can be one of the following:
		0 - Vertical stack
		1 - Horizontal stack
		2 - Grid
		3 - Fullscreen
-left	[pos]	Left position of display, a monitor to the left of the main display will be a negative number, eg. -1024
-top 	[pos]	Top position of display, eg. 0
-width	[pos]	Width of display, eg. 1024
-height [pos]	Height of Display, eg. 768
-x		Enable experimental mouse polling for activating hotkeys based on mouse position
-b		Reverse the behavior of -bi (Only classes specified have borders removed)

If display positions aren't specified on the command line then the workspace area is used instead.


Example shortcuts, (for two displays):
Left: HashTWM.exe -x -left -1024 -top 0 -width 1024 -height 768 -i iTunes -i ConsoleWindowClass -i #32770
Right: HashTWM.exe -x -left 0 -top 0 -width 1024 -height 768 -i iTunes -i ConsoleWindowClass -i #32770

A useful class to ignore is "#32770" which will ignore all message boxes.

Quirks:
If HashTWM appears to freeze with 100% CPU this is almost definately a bug in the handling of linked lists. Sorry, I'm relatively new to all this, but send me an email and let me know.
If experimental mouse polling isn't enabled then you will need to set different mod keys for each instance of HashTWM.
All Windows should be restored (ie. not minimized) before HashTWM is started. Otherwise, these windows will be taken into account in the tiling arrangement, but will stay minimized.
Grid mode doesn't quite work right, last window isn't always tiled correctly, likely a mistake in my port. Other than that it works great.