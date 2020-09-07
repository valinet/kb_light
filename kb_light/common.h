#pragma once
#define KEYBOARD_BACKLIGHT_RESTORE		255
#define KEYBOARD_BACKLIGHT_DISABLED		0
#define KEYBOARD_BACKLIGHT_DIM			1
#define KEYBOARD_BACKLIGHT_BRIGHT		2
#define SERVICE_NAME					TEXT("kb_light")
#define CH(x)							(x + '0')

VOID WINAPI ServiceMain(
	DWORD argc, 
	LPTSTR* argv
);

INT WINAPI ApplicationMain(
	HINSTANCE hInstance,
	HINSTANCE hPrevInstance,
	PWSTR pCmdLine,
	INT nCmdShow
);