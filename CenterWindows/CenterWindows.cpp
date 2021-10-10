#include <math.h>
#include <stdio.h>
#include <Windows.h>

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

BOOL CenterWindow(HWND hWnd, LONG taskbarHeight) {

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

	// calculate the new window position
	auto top = floor(resolution.rcMonitor.top + ((resolution.rcMonitor.bottom - taskbarHeight) - resolution.rcMonitor.top - height) / 2.0);
	auto left = floor(resolution.rcMonitor.left + (resolution.rcMonitor.right - resolution.rcMonitor.left - width) / 2.0);

	// move the window to the calculate position
	return SetWindowPos(hWnd, NULL, left, top, width, height, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

BOOL CALLBACK EnumWindowsProc(HWND hWnd, LPARAM lParam) {

	// we don't want to center anything that is not a window, and is not
	// currently visible. so lets check for those two scenarios.
	if (!IsWindow(hWnd) || !IsWindowVisible(hWnd)) {
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
	if (CenterWindow(hWnd, lParam) == FALSE) {
		printf("failed to set window position (%ld)!\n", GetLastError());
	}

	// allow centering of more windows
	return TRUE;
}

int main() {

	// locate task bar
	auto taskbar = FindWindow("Shell_TrayWnd", NULL);
	if (taskbar == NULL) {
		return GetLastError();
	}

	// get taskbar dimensions
	RECT taskbarDimensions;
	if (GetWindowRect(taskbar, &taskbarDimensions) == FALSE) {
		return GetLastError();
	}

	// get the current foreground window
	auto foreground = GetForegroundWindow();

	// taskbar resolution difference
	auto taskbarHeight = taskbarDimensions.bottom - taskbarDimensions.top;

	// handle each window
	if (EnumWindows(EnumWindowsProc, taskbarHeight) == FALSE) {
		return GetLastError();
	}

	// bring proper window to foreground
	if (foreground != NULL) {
		SetForegroundWindow(foreground);
	}
}
