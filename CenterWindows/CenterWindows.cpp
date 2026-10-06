#include <fcntl.h>
#include <io.h>
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

BOOL CenterWindow(HWND hWnd) {

	// get the dimensions of the window
	RECT dimensions;
	if (GetWindowRect(hWnd, &dimensions) == FALSE) {
		wprintf(L"failed to get window rect (%lu)!\n", GetLastError());
		return false;
	}

	// on windows 10+ the window rect includes invisible resize borders on the
	// left, right and bottom (but not the top), so center the visible frame
	// instead. fall back to the window rect if dwm can't tell us.
	RECT frame;
	if (FAILED(DwmGetWindowAttribute(hWnd, DWMWA_EXTENDED_FRAME_BOUNDS, &frame, sizeof(frame)))) {
		frame = dimensions;
	}

	// calculate visible width and height
	auto width = frame.right - frame.left;
	auto height = frame.bottom - frame.top;

	// get the current monitor for the window
	auto monitor = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);

	MONITORINFO resolution;

	// must set cbSize before calling GetMonitorInfo
	resolution.cbSize = sizeof(MONITORINFO);

	// get resolution details for the monitor
	if (GetMonitorInfoW(monitor, &resolution) == FALSE) {
		wprintf(L"failed to get monitor info (%lu)!\n", GetLastError());
		return false;
	}

	// the work area is the monitor minus the taskbar and any docked app bars,
	// wherever they are on this particular monitor
	const RECT& work = resolution.rcWork;

	// calculate the new position of the visible frame. if the window is larger
	// than the work area, pin it to the top left so the title bar stays
	// reachable.
	LONG top = max(work.top, work.top + ((work.bottom - work.top) - height) / 2);
	LONG left = max(work.left, work.left + ((work.right - work.left) - width) / 2);

	// shift back out by the invisible borders to get the window position
	top -= frame.top - dimensions.top;
	left -= frame.left - dimensions.left;

	// move the window to the calculated position. async so a hung app can't
	// block us.
	if (SetWindowPos(hWnd, NULL, left, top, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_ASYNCWINDOWPOS) == FALSE) {
		wprintf(L"failed to set window position (%lu)!\n", GetLastError());
		return false;
	}

	return true;
}

BOOL CALLBACK EnumWindowsProc(HWND hWnd, LPARAM lParam) {

	// we don't want to center anything that is not currently visible
	if (!IsWindowVisible(hWnd)) {
		return TRUE;
	}

	// skip windows that are "visible" but cloaked, such as those on another
	// virtual desktop
	if (IsWindowCloaked(hWnd)) {
		return TRUE;
	}

	// we also don't want to touch windows that are either minimized or
	// maximized
	if (IsIconic(hWnd) || IsZoomed(hWnd)) {
		return TRUE;
	}

	// if there's no title then skip the window i guess
	if (GetWindowTextLengthW(hWnd) == 0) {
		return TRUE;
	}

	// get window text
	wchar_t title[256];
	GetWindowTextW(hWnd, title, ARRAYSIZE(title));

	wprintf(L"centering `%ls` ...\n", title);

	// center the given window
	CenterWindow(hWnd);

	// allow centering of more windows
	return TRUE;
}

int main() {

	// write utf-16 to the console so non-ascii window titles print correctly.
	// after this, only the wide printf functions may be used on stdout.
	_setmode(_fileno(stdout), _O_U16TEXT);

	// opt into per-monitor dpi awareness so windows doesn't virtualize the
	// coordinates we see on scaled or mixed-dpi displays
	SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

	// handle each window
	if (EnumWindows(EnumWindowsProc, 0) == FALSE) {
		return GetLastError();
	}
}
