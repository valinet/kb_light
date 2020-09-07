#include <Windows.h>
#include <stdio.h>
#include <tlhelp32.h>
#define DRIVER_IBMPMDRV					0
#define DRIVER_WINRING0					1
#define DRIVER DRIVER_IBMPMDRV
#define TIMEOUT_CREATEPROCESS			5000
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

DWORD WINAPI ServiceWorkerThread(
	LPVOID lpParam
)
{
	if (Initialize())
	{
		return NULL;
	}

	HANDLE jobHandle = CreateJobObject(
		NULL,
		NULL
	);
	JOBOBJECT_BASIC_LIMIT_INFORMATION jobInfoBasic;
	jobInfoBasic.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
	JOBOBJECT_EXTENDED_LIMIT_INFORMATION jobInfo;
	jobInfo.BasicLimitInformation = jobInfoBasic;
	SetInformationJobObject(
		jobHandle,
		JobObjectExtendedLimitInformation, &jobInfo,
		sizeof(JOBOBJECT_EXTENDED_LIMIT_INFORMATION)
	);

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
		PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
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
	while (TRUE)
	{
		HANDLE winLogon = NULL;
		PROCESSENTRY32 entry;
		entry.dwSize = sizeof(PROCESSENTRY32);
		HANDLE snapshot = CreateToolhelp32Snapshot(
			TH32CS_SNAPPROCESS,
			NULL
		);
		if (Process32First(snapshot, &entry) == TRUE)
		{
			while (Process32Next(snapshot, &entry) == TRUE)
			{
				if (!wcscmp(entry.szExeFile, L"winlogon.exe"))
				{
					if (!(winLogon = OpenProcess(
						PROCESS_ALL_ACCESS,
						FALSE,
						entry.th32ProcessID
					)))
					{
						return NULL;
					}
					break;
				}
			}
		}
		CloseHandle(snapshot);

		HANDLE userToken;
		if (!OpenProcessToken
		(
			winLogon,
			TOKEN_QUERY | TOKEN_IMPERSONATE | TOKEN_DUPLICATE,
			&userToken)
			)
		{
			return NULL;
		}

		HANDLE newToken = 0;
		SECURITY_ATTRIBUTES tokenAttributes = { 0 };
		tokenAttributes.nLength = sizeof(SECURITY_ATTRIBUTES);
		SECURITY_ATTRIBUTES threadAttributes = { 0 };
		threadAttributes.nLength = sizeof(SECURITY_ATTRIBUTES);

		if (!DuplicateTokenEx
		(
			userToken,
			0x10000000,
			&tokenAttributes,
			SecurityImpersonation,
			TokenImpersonation,
			&newToken
		))
		{
			return NULL;
		}

		TOKEN_PRIVILEGES tokPrivs = { 0 };
		tokPrivs.PrivilegeCount = 1;
		LUID seDebugNameValue = { 0 };
		if (!LookupPrivilegeValue
		(
			NULL,
			SE_DEBUG_NAME,
			&seDebugNameValue
		))
		{
			return NULL;
		}

		tokPrivs.Privileges[0].Luid = seDebugNameValue;
		tokPrivs.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
		if (!AdjustTokenPrivileges(
			newToken,
			FALSE,
			&tokPrivs,
			0,
			NULL,
			NULL
		)) {
			return NULL;
		}

		TCHAR szFileName[_MAX_PATH + 2];
		szFileName[0] = '\"';
		GetModuleFileName(
			GetModuleHandle(NULL),
			szFileName + 1,
			_MAX_PATH
		);
		lstrcat(
			szFileName,
			L"\" d"
		);

		while (TRUE)
		{
			PROCESS_INFORMATION pi = { 0 };
			STARTUPINFO si = { 0 };
			si.cb = sizeof(STARTUPINFO);
			si.lpDesktop = TEXT("WinSta0\\Default");
			// start the process using the new token
			if (!CreateProcessAsUser(
				newToken,
				NULL,
				szFileName,
				&tokenAttributes,
				&threadAttributes,
				TRUE,
				CREATE_NEW_CONSOLE | INHERIT_CALLER_PRIORITY,
				NULL,
				NULL,
				&si,
				&pi
			))
			{
				DWORD dwRes = WaitForSingleObject(
					g_ServiceStopEvent,
					TIMEOUT_CREATEPROCESS
				);
				if (dwRes == WAIT_OBJECT_0 + 0)
				{
					CloseHandle(hPipe);
					CloseHandle(hEvent);
					CloseHandle(jobHandle);
					Uninitialize();
					return NULL;
				}
				break;
			}
			AssignProcessToJobObject(
				jobHandle,
				pi.hProcess
			);

			while (TRUE)
			{
				HANDLE handles[3];
				handles[0] = hEvent;
				handles[1] = g_ServiceStopEvent;
				handles[2] = pi.hProcess;
				DWORD dwRes = WaitForMultipleObjects(
					3,
					&handles,
					FALSE,
					INFINITE
				);
				if (dwRes == WAIT_OBJECT_0 + 2)
				{
					break;
				}
				else if (dwRes == WAIT_OBJECT_0 + 1)
				{
					CloseHandle(pi.hProcess);
					CloseHandle(hPipe);
					CloseHandle(hEvent);
					CloseHandle(jobHandle);
					Uninitialize();
					return NULL;
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
							if (buffer)
							{
								if (buffer == CH(KEYBOARD_BACKLIGHT_DISABLED))
								{
									value = SetKeyboardBacklight(KEYBOARD_BACKLIGHT_DISABLED);
								}
								else if (buffer == CH(KEYBOARD_BACKLIGHT_DIM))
								{
									value = SetKeyboardBacklight(KEYBOARD_BACKLIGHT_DIM);
								}
								else if (buffer == CH(KEYBOARD_BACKLIGHT_BRIGHT))
								{
									value = SetKeyboardBacklight(KEYBOARD_BACKLIGHT_BRIGHT);
								}
								else if (buffer == KEYBOARD_BACKLIGHT_RESTORE)
								{
									SetKeyboardBacklight(value);
								}
							}
							else
							{
								BYTE val = GetKeyboardBacklight();
								if (val == KEYBOARD_BACKLIGHT_DISABLED)
								{
									buffer = CH(KEYBOARD_BACKLIGHT_DISABLED);
								}
								else if (val == KEYBOARD_BACKLIGHT_DIM)
								{
									buffer = CH(KEYBOARD_BACKLIGHT_DIM);
								}
								else if (val == KEYBOARD_BACKLIGHT_BRIGHT)
								{
									buffer = CH(KEYBOARD_BACKLIGHT_BRIGHT);
								}
								result = WriteFile(
									hPipe,
									&buffer,
									1 * sizeof(TCHAR),
									NULL,
									&oOverlap
								);
								if (result)
								{
									FlushFileBuffers(hEvent);
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
			CloseHandle(pi.hProcess);
		}
	}

	CloseHandle(hPipe);
	CloseHandle(hEvent);
	CloseHandle(jobHandle);
	Uninitialize();
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