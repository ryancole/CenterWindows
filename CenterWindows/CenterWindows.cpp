#include <math.h>
#include <tchar.h>
#include <Windows.h>

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
		_tprintf(L"failed to get monitor info (%ld)!\n", GetLastError());
		return false;
	}

	// calculate the new window position
	auto top = floor(resolution.rcMonitor.top + ((resolution.rcMonitor.bottom - taskbarHeight) - resolution.rcMonitor.top - height) / 2.0);
	auto left = floor(resolution.rcMonitor.left + (resolution.rcMonitor.right - resolution.rcMonitor.left - width) / 2.0);

	// move the window to the calculate position
	return SetWindowPos(hWnd, NULL, left, top, width, height, SWP_NOZORDER);
}

BOOL CALLBACK EnumWindowsProc(HWND hWnd, LPARAM lParam) {

	// get the window's style information
	auto style = GetWindowLong(hWnd, GWL_STYLE);
	if (style == 0 || (style & (WS_CAPTION | WS_VISIBLE)) != (WS_CAPTION | WS_VISIBLE)) {
		return TRUE;
	}

	// get window text
	TCHAR title[64];
	GetWindowText(hWnd, title, 64);

	// let user know which window
	_tprintf(L"centering `%s` ...\n", title);

	// center the given window
	if (CenterWindow(hWnd, lParam) == FALSE) {
		_tprintf(L"failed to set window position (%ld)!\n", GetLastError());
	}

	// allow centering of more windows
	return TRUE;
}

int main() {

	// locate task bar
	auto taskbar = FindWindow(L"Shell_TrayWnd", NULL);
	if (taskbar == NULL) {
		return GetLastError();
	}

	// get taskbar dimensions
	RECT taskbarDimensions;
	if (GetWindowRect(taskbar, &taskbarDimensions) == FALSE) {
		return GetLastError();
	}

	// taskbar resolution difference
	auto taskbarHeight = taskbarDimensions.bottom - taskbarDimensions.top;

	// handle each window
	if (EnumWindows(EnumWindowsProc, taskbarHeight) == FALSE) {
		return GetLastError();
	}
}
