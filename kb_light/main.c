#include <Windows.h>
#include <stdio.h>
#include <shlwapi.h>
#pragma comment(lib, "Shlwapi.lib")
#include <Lmcons.h>
#define DRIVER_IBMPMDRV					0
#define DRIVER_WINRING0					1
#define DRIVER DRIVER_IBMPMDRV
#define TIMEOUT							2000
#if DRIVER == DRIVER_IBMPMDRV
#include "IbmPmDrv.h"
#elif DRIVER == DRIVER_WINRING0
#include "WinRing0.h"
#endif

SERVICE_STATUS        g_ServiceStatus = { 0 };
SERVICE_STATUS_HANDLE g_StatusHandle = NULL;
HANDLE                g_ServiceStopEvent = INVALID_HANDLE_VALUE;

VOID WINAPI ServiceMain(DWORD argc, LPTSTR* argv);
VOID WINAPI ServiceCtrlHandler(DWORD);
DWORD WINAPI ServiceWorkerThread(LPVOID lpParam);

#define SERVICE_NAME TEXT("keybd_light")  

DWORD WINAPI ServiceWorkerThread(
	LPVOID lpParam
)
{
	if (Initialize())
	{
		return NULL;
	}

	// https://support.microsoft.com/en-my/help/110148/prb-error-invalid-parameter-from-writefile-or-readfile
	OVERLAPPED oOverlap = { 0 };

	HANDLE hEvent = CreateEvent(
		NULL,
		TRUE,
		TRUE,
		NULL
	);
	if (!hEvent)
	{
		return NULL;
	}
	oOverlap.hEvent = hEvent;

	SECURITY_DESCRIPTOR sd;
	InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION);
	SetSecurityDescriptorDacl(&sd, TRUE, NULL, FALSE);

	SECURITY_ATTRIBUTES sa = { 0 };
	sa.nLength = sizeof(SECURITY_ATTRIBUTES);
	sa.lpSecurityDescriptor = &sd;
	sa.bInheritHandle = FALSE;

	HANDLE hPipe = CreateNamedPipe(
		L"\\\\.\\pipe\\keybd_light",
		PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED,
		PIPE_TYPE_BYTE,
		1,
		0,
		0,
		0,
		&sa
	);
	if (hPipe == INVALID_HANDLE_VALUE)
	{
		return NULL;
	}

	BOOL bConnect = ConnectNamedPipe(
		hPipe,
		&oOverlap
	);
	if (bConnect)
	{
		return NULL;
	}
	switch (GetLastError())
	{
	case ERROR_IO_PENDING:
		break;
	case ERROR_PIPE_CONNECTED:
		if (SetEvent(oOverlap.hEvent))
			break;
	default:
		return NULL;
	}

	BYTE value = 0;
	QUERY_USER_NOTIFICATION_STATE state, prev_state;
	SHQueryUserNotificationState(
		&prev_state
	);
	while (TRUE)
	{
		SHQueryUserNotificationState(
			&state
		);
		if (state != prev_state)
		{
			if (state == QUNS_BUSY)
			{
				value = GetKeyboardBacklight();
				SetKeyboardBacklight(KEYBOARD_BACKLIGHT_DISABLED);
			}
			else
			{
				SetKeyboardBacklight(value);
			}
		}
		prev_state = state;
		HANDLE handles[2];
		handles[0] = hEvent;
		handles[1] = g_ServiceStopEvent;
		DWORD dwRes = WaitForMultipleObjects(
			2,
			&handles,
			FALSE,
			TIMEOUT
		);
		if (dwRes == WAIT_OBJECT_0 + 1)
		{
			break;
		}
		else if (dwRes == WAIT_OBJECT_0 + 0)
		{
			TCHAR buffer = 0;
			BOOL result = ReadFile(
				hPipe,
				&buffer,
				1 * sizeof(TCHAR),
				NULL,
				&oOverlap
			);
			if (result)
			{
				DWORD numberOfBytesRead = 0;
				result = GetOverlappedResult(
					hPipe,
					&oOverlap,
					&numberOfBytesRead,
					TRUE
				);
				if (result && numberOfBytesRead)
				{
					if (buffer == 'x')
					{
						value = GetKeyboardBacklight();
						char szLibPath[_MAX_PATH];
						GetModuleFileNameA(
							GetModuleHandle(NULL),
							szLibPath,
							_MAX_PATH
						);
						PathRemoveFileSpecA(szLibPath);
						strcat_s(
							szLibPath,
							_MAX_PATH,
							"\\status"
						);
						FILE* f = NULL;
						fopen_s(&f, szLibPath, "w");
						fprintf(f, "%d\n", value == 0x40 ? 1 : (value == 0x80 ? 2 : 0));
						fclose(f);
					}
					else if (buffer == 'y')
					{
						char szLibPath[_MAX_PATH];
						GetModuleFileNameA(
							GetModuleHandle(NULL),
							szLibPath,
							_MAX_PATH
						);
						PathRemoveFileSpecA(szLibPath);
						strcat_s(
							szLibPath,
							_MAX_PATH,
							"\\status"
						);
						BYTE new_value = 0;
						FILE* f = NULL;
						fopen_s(&f, szLibPath, "r");
						if (f)
						{
							fscanf_s(f, "%d", &new_value);
							fclose(f);
							if (new_value == KEYBOARD_BACKLIGHT_DISABLED ||
								new_value == KEYBOARD_BACKLIGHT_DIM || 
								new_value == KEYBOARD_BACKLIGHT_BRIGHT
							)
							{
								value = new_value;
								SetKeyboardBacklight(value);
							}
						}
					}
				}
			}
			DisconnectNamedPipe(hPipe);
			BOOL bConnect = ConnectNamedPipe(
				hPipe,
				&oOverlap
			);
			if (bConnect)
			{
				return NULL;
			}
			switch (GetLastError())
			{
			case ERROR_IO_PENDING:
				break;
			case ERROR_PIPE_CONNECTED:
				if (SetEvent(oOverlap.hEvent))
					break;
			default:
				return NULL;
			}
		}
	}

	CloseHandle(hPipe);
	CloseHandle(hEvent);
	Deinitialize();

	return NULL;
}

VOID WINAPI ServiceCtrlHandler(DWORD CtrlCode)
{
	switch (CtrlCode)
	{
	case SERVICE_CONTROL_STOP:

		if (g_ServiceStatus.dwCurrentState != SERVICE_RUNNING)
			break;

		//
		// Perform tasks necessary to stop the service here
		//

		g_ServiceStatus.dwControlsAccepted = 0;
		g_ServiceStatus.dwCurrentState = SERVICE_STOP_PENDING;
		g_ServiceStatus.dwWin32ExitCode = 0;
		g_ServiceStatus.dwCheckPoint = 4;

		if (SetServiceStatus(g_StatusHandle, &g_ServiceStatus) == FALSE)
		{
			OutputDebugString(TEXT(
				"keybd_light: ServiceCtrlHandler: SetServiceStatus returned error"));
		}

		// This will signal the worker thread to start shutting down
		SetEvent(g_ServiceStopEvent);

		break;

	default:
		break;
	}
}

VOID WINAPI ServiceMain(
	DWORD argc,
	LPTSTR* argv
)
{
	DWORD Status = E_FAIL;

	// Register our service control handler with the SCM
	g_StatusHandle = RegisterServiceCtrlHandler(SERVICE_NAME, ServiceCtrlHandler);

	if (g_StatusHandle == NULL)
	{
		goto EXIT;
	}

	// Tell the service controller we are starting
	ZeroMemory(&g_ServiceStatus, sizeof(g_ServiceStatus));
	g_ServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
	g_ServiceStatus.dwControlsAccepted = 0;
	g_ServiceStatus.dwCurrentState = SERVICE_START_PENDING;
	g_ServiceStatus.dwWin32ExitCode = 0;
	g_ServiceStatus.dwServiceSpecificExitCode = 0;
	g_ServiceStatus.dwCheckPoint = 0;

	if (SetServiceStatus(g_StatusHandle, &g_ServiceStatus) == FALSE)
	{
		OutputDebugString(TEXT(
			"keybd_light: ServiceMain: SetServiceStatus returned error"));
	}

	//
	// Perform tasks necessary to start the service here
	//

	 // Create a service stop event to wait on later
	g_ServiceStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (g_ServiceStopEvent == NULL)
	{
		// Error creating event
		// Tell service controller we are stopped and exit
		g_ServiceStatus.dwControlsAccepted = 0;
		g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
		g_ServiceStatus.dwWin32ExitCode = GetLastError();
		g_ServiceStatus.dwCheckPoint = 1;

		if (SetServiceStatus(g_StatusHandle, &g_ServiceStatus) == FALSE)
		{
			OutputDebugString(TEXT(
				"keybd_light: ServiceMain: SetServiceStatus returned error"));
		}
		goto EXIT;
	}

	// Tell the service controller we are started
	g_ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;
	g_ServiceStatus.dwCurrentState = SERVICE_RUNNING;
	g_ServiceStatus.dwWin32ExitCode = 0;
	g_ServiceStatus.dwCheckPoint = 0;

	if (SetServiceStatus(g_StatusHandle, &g_ServiceStatus) == FALSE)
	{
		OutputDebugString(TEXT(
			"keybd_light: ServiceMain: SetServiceStatus returned error"));
	}

	// Start a thread that will perform the main task of the service
	HANDLE hThread = CreateThread(NULL, 0, ServiceWorkerThread, NULL, 0, NULL);

	// Wait until our worker thread exits signaling that the service needs to stop
	WaitForSingleObject(hThread, INFINITE);


	//
	// Perform any cleanup tasks
	//

	CloseHandle(g_ServiceStopEvent);

	// Tell the service controller we are stopped
	g_ServiceStatus.dwControlsAccepted = 0;
	g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
	g_ServiceStatus.dwWin32ExitCode = 0;
	g_ServiceStatus.dwCheckPoint = 3;

	if (SetServiceStatus(g_StatusHandle, &g_ServiceStatus) == FALSE)
	{
		OutputDebugString(TEXT(
			"keybd_light: ServiceMain: SetServiceStatus returned error"));
	}

EXIT:
	return;
}

BOOL IsLocalSystem()
{
	HANDLE hToken;
	UCHAR bTokenUser[sizeof(TOKEN_USER) + 8 + 4 * SID_MAX_SUB_AUTHORITIES];
	PTOKEN_USER pTokenUser = (PTOKEN_USER)bTokenUser;
	ULONG cbTokenUser;
	SID_IDENTIFIER_AUTHORITY siaNT = SECURITY_NT_AUTHORITY;
	PSID pSystemSid;
	BOOL bSystem;

	// open process token
	if (!OpenProcessToken(GetCurrentProcess(),
		TOKEN_QUERY,
		&hToken))
		return FALSE;

	// retrieve user SID
	if (!GetTokenInformation(hToken, TokenUser, pTokenUser,
		sizeof(bTokenUser), &cbTokenUser))
	{
		CloseHandle(hToken);
		return FALSE;
	}

	CloseHandle(hToken);

	// allocate LocalSystem well-known SID
	if (!AllocateAndInitializeSid(&siaNT, 1, SECURITY_LOCAL_SYSTEM_RID,
		0, 0, 0, 0, 0, 0, 0, &pSystemSid))
		return FALSE;

	// compare the user SID from the token with the LocalSystem SID
	bSystem = EqualSid(pTokenUser->User.Sid, pSystemSid);

	FreeSid(pSystemSid);

	return bSystem;
}

int WINAPI wWinMain(
	HINSTANCE hInstance,
	HINSTANCE hPrevInstance,
	PWSTR pCmdLine,
	int nCmdShow
)
{
	SERVICE_TABLE_ENTRY ServiceTable[] =
	{
		{SERVICE_NAME, (LPSERVICE_MAIN_FUNCTION)ServiceMain},
		{NULL, NULL}
	};

	if (StartServiceCtrlDispatcher(ServiceTable) == FALSE)
	{
		HANDLE hPipe = CreateFile(
			L"\\\\.\\pipe\\keybd_light",
			GENERIC_WRITE,
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

		TCHAR buffer = 0;
		if (__argc > 1)
		{
			buffer = 'x';
		}
		else
		{
			buffer = 'y';
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
		CloseHandle(hPipe);
	}

	return 0;
}