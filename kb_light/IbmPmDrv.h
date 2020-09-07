#pragma once
#include <Windows.h>
#include "common.h"

#define DRIVER_IBMPMDRV_CUSTOM_MASK		0x00200000
#define DRIVER_IBMPMDRV_READY_MASK		0x0050000
#define DRIVER_IBMPMDRV_WRITE_MASK		0x100
#define DRIVER_IBMPMDRV_READ_LOCATION	2238080
#define DRIVER_IBMPMDRV_WRITE_LOCATION	2238084

BYTE GetKeyboardBacklight()
{
	BYTE status = 0;
	HANDLE hFile = CreateFile(
		L"\\\\.\\IBMPmDrv",
		GENERIC_READ,
		FILE_SHARE_READ,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL
	);
	if (hFile == INVALID_HANDLE_VALUE)
	{
		return status;
	}
	UINT rcv = 0, snt = 0;
	DWORD bytesReturned = 0;
	BOOL bRet = DeviceIoControl(
		hFile,
		DRIVER_IBMPMDRV_READ_LOCATION,
		&snt,
		sizeof(snt),
		&rcv,
		sizeof(rcv),
		&bytesReturned,
		NULL
	);
	if (bytesReturned && (rcv & DRIVER_IBMPMDRV_READY_MASK))
	{
		status = (rcv >> 8) & 0xF;
	}
	CloseHandle(hFile);
	return status;
}

VOID SetKeyboardBacklight(BYTE status)
{
	if (status != KEYBOARD_BACKLIGHT_DISABLED &&
		status != KEYBOARD_BACKLIGHT_DIM &&
		status != KEYBOARD_BACKLIGHT_BRIGHT
		)
	{
		return;
	}
	HANDLE hFile = CreateFile(
		L"\\\\.\\IBMPmDrv",
		GENERIC_READ,
		FILE_SHARE_READ,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL
	);
	if (hFile == INVALID_HANDLE_VALUE)
	{
		return;
	}
	UINT rcv = 0, snt = 0;
	DWORD bytesReturned = 0;
	BOOL bRet = DeviceIoControl(
		hFile,
		DRIVER_IBMPMDRV_READ_LOCATION,
		&snt,
		sizeof(snt),
		&rcv,
		sizeof(rcv),
		&bytesReturned,
		NULL
	);
	if (bytesReturned && (rcv & DRIVER_IBMPMDRV_READY_MASK))
	{
		snt = ((rcv & DRIVER_IBMPMDRV_CUSTOM_MASK) != 0 ? DRIVER_IBMPMDRV_WRITE_MASK : 0)
			| (rcv & 0xF0)
			| status;
		rcv = 0;
		bRet = DeviceIoControl(
			hFile,
			DRIVER_IBMPMDRV_WRITE_LOCATION,
			&snt,
			sizeof(snt),
			&rcv,
			sizeof(rcv),
			&bytesReturned,
			NULL
		);
	}
	CloseHandle(hFile);
}

DWORD Initialize()
{
	return 0;
}

VOID Deinitialize()
{
	return;
}