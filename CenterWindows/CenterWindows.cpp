#include <stdio.h>
#include <Windows.h>
#include <dwmapi.h>

#pragma comment(lib, "dwmapi.lib")

BOOL IsWindowCloaked(HWND hWnd) {
	DWORD cloaked = 0;

	// windows on other virtual desktops and some hidden uwp frames report as
	// visible, but dwm has them cloaked so they aren't actually on screen
	if (FAILED(DwmGetWindowAttribute(hWnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked)))) {
		return false;
	}

	return cloaked != 0;
}

BOOL IsWindowNormalState(HWND hWnd) {
	WINDOWPLACEMENT placement;

	placement.length = sizeof(WINDOWPLACEMENT);

	if (GetWindowPlacement(hWnd, &placement) == 0) {
		printf("failed to get window placement (%ld)!\n", GetLastError());
		return false;
	}

	// a normal window is one that is not minimized or maximized
	if (placement.showCmd == SW_SHOWNORMAL) {
		return true;
	}

	return false;
}

BOOL CenterWindow(HWND hWnd) {

	// get the dimensions of the window
	RECT dimensions;
	if (GetWindowRect(hWnd, &dimensions) == 0) {
		return false;
	}

	// calculate width and height
	auto width = dimensions.right - dimensions.left;
	auto height = dimensions.bottom - dimensions.top;

	// get the current monitor for the window
	auto monitor = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);

	MONITORINFO resolution;

	// must set cbSize before calling GetMonitorInfo
	resolution.cbSize = sizeof(MONITORINFO);

	// get resolution details for the monitor
	if (GetMonitorInfo(monitor, &resolution) == FALSE) {
		printf("failed to get monitor info (%ld)!\n", GetLastError());
		return false;
	}

	// the work area is the monitor minus the taskbar and any docked app bars,
	// wherever they are on this particular monitor
	const RECT& work = resolution.rcWork;

	// calculate the new window position
	LONG top = work.top + ((work.bottom - work.top) - height) / 2;
	LONG left = work.left + ((work.right - work.left) - width) / 2;

	// move the window to the calculate position
	return SetWindowPos(hWnd, NULL, left, top, width, height, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

BOOL CALLBACK EnumWindowsProc(HWND hWnd, LPARAM lParam) {

	// we don't want to center anything that is not a window, and is not
	// currently visible. so lets check for those two scenarios.
	if (!IsWindow(hWnd) || !IsWindowVisible(hWnd)) {
		return TRUE;
	}

	// skip windows that are "visible" but cloaked, such as those on another
	// virtual desktop
	if (IsWindowCloaked(hWnd)) {
		return TRUE;
	}

	// we also don't want to touch windows that are either minimized or
	// maximized
	if (!IsWindowNormalState(hWnd)) {
		return TRUE;
	}

	// get window text
	TCHAR title[64];
	GetWindowText(hWnd, title, 64);

	// if there's no title then skip the window i guess
	if (strlen(title) == 0) {
		return TRUE;
	}

	printf("centering `%s` ...\n", title);

	// center the given window
	if (CenterWindow(hWnd) == FALSE) {
		printf("failed to set window position (%ld)!\n", GetLastError());
	}

	// allow centering of more windows
	return TRUE;
}

int main() {

	// opt into per-monitor dpi awareness so windows doesn't virtualize the
	// coordinates we see on scaled or mixed-dpi displays
	SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

	// get the current foreground window
	auto foreground = GetForegroundWindow();

	// handle each window
	if (EnumWindows(EnumWindowsProc, 0) == FALSE) {
		return GetLastError();
	}

	// bring proper window to foreground
	if (foreground != NULL) {
		SetForegroundWindow(foreground);
	}
}
