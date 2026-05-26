#include <Windows.h>
#include "common.h"
#define TIMEOUT_FULLSCREEN_CHECK		2000
#define SETTINGS_LOCATION 				L"SOFTWARE\\VALINET Solutions SRL\\kb_light"
#define ARG_QUERY						'?'
#define ARG_FULLSCREEN_QUERY			'd'

static LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	static UINT hookMsg = 0;
	static UINT atomRdp = 0;
	static TCHAR buffer = 0;
	static QUERY_USER_NOTIFICATION_STATE prev_state = 0;

	if (hookMsg == 0)
		hookMsg = RegisterWindowMessageW(L"SHELLHOOK");
	if (atomRdp == 0)
		atomRdp = RegisterWindowMessageW(L"TscShellContainerClass");
	if (uMsg == hookMsg && (wParam == 0x35 || wParam == 0x36) && // https://github.com/dechamps/RudeWindowFixer
		GetClassWord(GetForegroundWindow(), GCW_ATOM) != atomRdp) { // Exclude full screen Remote Desktop windows
		SetTimer(hWnd, wParam, 50, NULL);
	}
	else if (uMsg == WM_TIMER) {
		KillTimer(hWnd, wParam);
		QUERY_USER_NOTIFICATION_STATE state = 0;
		SHQueryUserNotificationState(&state);
		if (state != prev_state && (state == QUNS_BUSY || prev_state == QUNS_BUSY)) {
			HANDLE hPipe = CreateFileW(L"\\\\.\\pipe\\keybd_light", GENERIC_WRITE,
				FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
			if (hPipe == INVALID_HANDLE_VALUE)
				return DefWindowProcW(hWnd, uMsg, wParam, lParam);
			buffer = (state == QUNS_BUSY) ? CH(KEYBOARD_BACKLIGHT_DISABLED) : KEYBOARD_BACKLIGHT_RESTORE;
			DWORD numBytesWritten = 0;
			BOOL result = WriteFile(hPipe, &buffer, sizeof(TCHAR), &numBytesWritten, NULL);
			CloseHandle(hPipe);
		}
		prev_state = state;
	}
	return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

INT WINAPI ApplicationMain(
	HINSTANCE hInstance,
	HINSTANCE hPrevInstance,
	PWSTR pCmdLine,
	INT nCmdShow
)
{
	TCHAR buffer = 0;

	if (__argc > 1)
	{
		DWORD argc = 0;
		LPWSTR* argv = CommandLineToArgvW(pCmdLine, &argc);
		if (argv[0][0] == CH(KEYBOARD_BACKLIGHT_DISABLED))
		{
			LocalFree(argv);
			if (!Initialize())
			{
				SetKeyboardBacklight(KEYBOARD_BACKLIGHT_DISABLED);
				Uninitialize();
			}
			return 0;
		}
		else if (argv[0][0] == CH(KEYBOARD_BACKLIGHT_DIM))
		{
			LocalFree(argv);
			if (!Initialize())
			{
				SetKeyboardBacklight(KEYBOARD_BACKLIGHT_DIM);
				Uninitialize();
			}
			return 0;
		}
		else if (argv[0][0] == CH(KEYBOARD_BACKLIGHT_BRIGHT))
		{
			LocalFree(argv);
			if (!Initialize())
			{
				SetKeyboardBacklight(KEYBOARD_BACKLIGHT_BRIGHT);
				Uninitialize();
			}
			return 0;
		}
		else if (argv[0][0] == ARG_QUERY)
		{
			LocalFree(argv);
			if (!Initialize())
			{
				BYTE value = GetKeyboardBacklight(KEYBOARD_BACKLIGHT_BRIGHT);
				Uninitialize();
				return value;
			}
			return 0;
		}
		else if (argv[0][0] == ARG_FULLSCREEN_QUERY)
		{
			LocalFree(argv);

			WNDCLASSW wc = { 0 };
			wc.lpfnWndProc = WndProc;
			wc.hInstance = hInstance;
			wc.lpszClassName = L"{104B1C5D-E019-4179-AFA2-8252EF94BB16}";

			ATOM atom = RegisterClassW(&wc);
			if (!atom)
				return GetLastError();
			
			HWND hWnd = CreateWindowExW(0, atom, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, hInstance, NULL);
			if (!hWnd)
				return GetLastError();

			if (!RegisterShellHookWindow(hWnd))
				return GetLastError();

			MSG msg;
			while (GetMessageW(&msg, NULL, 0, 0) > 0) {
				TranslateMessage(&msg);
				DispatchMessageW(&msg);
			}

			DeregisterShellHookWindow(hWnd);
		}
		LocalFree(argv);
	}

	if (__argc <= 1)
	{
		HANDLE key;
		if (RegOpenKeyEx(
			HKEY_CURRENT_USER,
			SETTINGS_LOCATION,
			0,
			KEY_READ,
			&key
		) != ERROR_SUCCESS)
		{
			return 0;
		}
		DWORD32 value = 0;
		DWORD size = sizeof(DWORD32);
		DWORD type = REG_DWORD;
		if (RegQueryValueEx(
			key,
			NULL,
			NULL,
			&type,
			&value,
			&size
		) != ERROR_SUCCESS)
		{
			RegCloseKey(key);
			return 0;
		}
		RegCloseKey(key);
		if (value == 0)
		{
			buffer = CH(KEYBOARD_BACKLIGHT_DISABLED);
		}
		else if (value == 1)
		{
			buffer = CH(KEYBOARD_BACKLIGHT_DIM);
		}
		else if (value == 2)
		{
			buffer = CH(KEYBOARD_BACKLIGHT_BRIGHT);
		}
	}

	HANDLE hPipe = CreateFile(
		L"\\\\.\\pipe\\keybd_light",
		GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL
	);
	int x = GetLastError();
	if (hPipe == INVALID_HANDLE_VALUE)
	{
		return NULL;
	}
	DWORD numBytesWritten = 0;
	BOOL result = WriteFile(
		hPipe,
		&buffer,
		1 * sizeof(TCHAR),
		&numBytesWritten,
		NULL
	);
	if (!result || numBytesWritten == 0)
	{
		return 0;
	}
	if (__argc > 1)
	{
		DWORD numberOfBytesRead = 0;
		result = ReadFile(
			hPipe,
			&buffer,
			1 * sizeof(TCHAR),
			&numberOfBytesRead,
			NULL
		);
		int xxx = GetLastError();
		if (!result || numberOfBytesRead == 0)
		{
			return 0;
		}
		if (buffer != CH(KEYBOARD_BACKLIGHT_DISABLED) &&
			buffer != CH(KEYBOARD_BACKLIGHT_DIM) &&
			buffer != CH(KEYBOARD_BACKLIGHT_BRIGHT)
			)
		{
			return 0;
		}
		DWORD status = 0;
		HANDLE key;
		if (RegCreateKeyEx(
			HKEY_CURRENT_USER,
			SETTINGS_LOCATION,
			NULL,
			NULL,
			REG_OPTION_NON_VOLATILE,
			KEY_WRITE,
			NULL,
			&key,
			&status
		) != ERROR_SUCCESS)
		{
			return 0;
		}
		DWORD32 value;
		if (buffer == CH(KEYBOARD_BACKLIGHT_DISABLED))
		{
			value = KEYBOARD_BACKLIGHT_DISABLED;
		}
		else if (buffer == CH(KEYBOARD_BACKLIGHT_DIM))
		{
			value = KEYBOARD_BACKLIGHT_DIM;
		}
		else if (buffer == CH(KEYBOARD_BACKLIGHT_BRIGHT))
		{
			value = KEYBOARD_BACKLIGHT_BRIGHT;
		}
		if (RegSetValueEx(
			key,
			NULL,
			NULL,
			REG_DWORD,
			&value,
			sizeof(DWORD32)
		) != ERROR_SUCCESS)
		{
			RegCloseKey(key);
			return 0;
		}
		RegCloseKey(key);
	}
	CloseHandle(hPipe);
}