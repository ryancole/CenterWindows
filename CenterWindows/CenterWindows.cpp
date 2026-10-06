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

BOOL IsAppWindow(HWND hWnd) {

	// the desktop (program manager) is a visible, titled window, but moving it
	// is a very bad idea
	if (hWnd == GetShellWindow()) {
		return false;
	}

	// roughly the same rules alt+tab uses to decide what counts as an app
	// window. tool windows are floating palettes and overlays.
	auto exStyle = GetWindowLongPtrW(hWnd, GWL_EXSTYLE);
	if (exStyle & WS_EX_TOOLWINDOW) {
		return false;
	}

	// windows can force themselves to be treated as app windows
	if (exStyle & WS_EX_APPWINDOW) {
		return true;
	}

	// owned windows are dialogs and popups that belong to another window, and
	// no-activate windows are things like notifications and overlays
	if (GetWindow(hWnd, GW_OWNER) != NULL || (exStyle & WS_EX_NOACTIVATE)) {
		return false;
	}

	return true;
}

BOOL IsWindowFullscreen(HWND hWnd) {

	// borderless fullscreen windows (games, video) aren't maximized, they just
	// cover the entire monitor
	RECT dimensions;
	if (GetWindowRect(hWnd, &dimensions) == FALSE) {
		return false;
	}

	MONITORINFO resolution;
	resolution.cbSize = sizeof(MONITORINFO);
	if (GetMonitorInfoW(MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST), &resolution) == FALSE) {
		return false;
	}

	const RECT& monitor = resolution.rcMonitor;

	return dimensions.left <= monitor.left && dimensions.top <= monitor.top
		&& dimensions.right >= monitor.right && dimensions.bottom >= monitor.bottom;
}

BOOL GetIntegrityLevel(HANDLE token, DWORD& level) {
	alignas(TOKEN_MANDATORY_LABEL) BYTE buffer[TOKEN_INTEGRITY_LEVEL_MAX_SIZE];
	DWORD size;

	if (GetTokenInformation(token, TokenIntegrityLevel, buffer, sizeof(buffer), &size) == FALSE) {
		return false;
	}

	// the integrity level is the last sub authority of the label sid
	auto sid = reinterpret_cast<TOKEN_MANDATORY_LABEL*>(buffer)->Label.Sid;
	level = *GetSidSubAuthority(sid, *GetSidSubAuthorityCount(sid) - 1);

	return true;
}

BOOL IsWindowHigherIntegrity(HWND hWnd, DWORD ourLevel) {

	// windows won't let a process move windows belonging to a process with a
	// higher integrity level (e.g. an app running as admin when we aren't).
	// if we're elevated ourselves this isn't a concern.
	if (ourLevel >= SECURITY_MANDATORY_HIGH_RID) {
		return false;
	}

	DWORD processId;
	GetWindowThreadProcessId(hWnd, &processId);

	auto process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
	if (process == NULL) {
		return GetLastError() == ERROR_ACCESS_DENIED;
	}

	// we usually aren't allowed to open an elevated process's token at all,
	// so treat access denied as elevated too
	HANDLE token;
	if (OpenProcessToken(process, TOKEN_QUERY, &token) == FALSE) {
		auto error = GetLastError();
		CloseHandle(process);
		return error == ERROR_ACCESS_DENIED;
	}

	DWORD level;
	auto higher = GetIntegrityLevel(token, level) && level > ourLevel;

	CloseHandle(token);
	CloseHandle(process);

	return higher;
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

	// skip the desktop, tool windows, dialogs and popups
	if (!IsAppWindow(hWnd)) {
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

	// leave snapped windows where the user put them
	if (IsWindowArranged(hWnd)) {
		wprintf(L"skipping `%ls` (snapped)\n", title);
		return TRUE;
	}

	if (IsWindowFullscreen(hWnd)) {
		wprintf(L"skipping `%ls` (fullscreen)\n", title);
		return TRUE;
	}

	// moves are async so we wouldn't find out they failed, check up front
	if (IsWindowHigherIntegrity(hWnd, static_cast<DWORD>(lParam))) {
		wprintf(L"skipping `%ls` (elevated, run as admin to center it)\n", title);
		return TRUE;
	}

	wprintf(L"centering `%ls` ...\n", title);

	// center the given window
	CenterWindow(hWnd);

	// allow centering of more windows
	return TRUE;
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {

	// this is a gui app so no console window flashes up when it's launched
	// from a shortcut or hotkey. if it was run from a terminal, write output
	// there instead.
	if (AttachConsole(ATTACH_PARENT_PROCESS)) {
		FILE* stream;
		if (_wfreopen_s(&stream, L"CONOUT$", L"w", stdout) == 0) {

			// write utf-16 to the console so non-ascii window titles print
			// correctly. after this, only the wide printf functions may be
			// used on stdout.
			_setmode(_fileno(stdout), _O_U16TEXT);
		}
	}

	// opt into per-monitor dpi awareness so windows doesn't virtualize the
	// coordinates we see on scaled or mixed-dpi displays
	SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

	// our own integrity level, so we can tell which windows we can't move
	DWORD integrityLevel = SECURITY_MANDATORY_MEDIUM_RID;
	GetIntegrityLevel(GetCurrentProcessToken(), integrityLevel);

	// handle each window
	if (EnumWindows(EnumWindowsProc, integrityLevel) == FALSE) {
		return GetLastError();
	}

	return 0;
}
