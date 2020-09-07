#pragma once
#include <Windows.h>
#include "common.h"

HMODULE hWinRing0 = NULL;
BOOL(*InitializeOls)() = NULL;
DWORD(*GetDllStatus)() = NULL;
VOID(*DeinitializeOls)() = NULL;
BYTE(*ReadIoPortByte)(USHORT port) = NULL;
VOID(*WriteIoPortByte)(USHORT port, BYTE value) = NULL;

// credits: http://read.pudn.com/downloads75/sourcecode/windows/vxd/279232/fancontrol/portio.cpp__.htm
// Registers of the embedded controller   
#define EC_DATAPORT     0x62    // EC data io-port   
#define EC_CTRLPORT     0x66    // EC control io-port   


// Embedded controller status register bits   
#define EC_STAT_OBF     0x01    // Output buffer full    
#define EC_STAT_IBF     0x02    // Input buffer full    
#define EC_STAT_CMD     0x08    // Last write was a command write (0=data)    


// Embedded controller commands   
// (write to EC_CTRLPORT to initiate read/write operation)   
#define EC_CTRLPORT_READ        (char)0x80     
#define EC_CTRLPORT_WRITE       (char)0x81   
#define EC_CTRLPORT_QUERY       (char)0x84   

#define DEFAULT_WAIT_TIMEOUT 1000

#define DRIVER_WINRING0_KEYBOARD_BACKLIGHT_OFFSET   0x0D
#define DRIVER_WINRING0_KEYBOARD_BACKLIGHT_DISABLED 0x00
#define DRIVER_WINRING0_KEYBOARD_BACKLIGHT_DIM      0x40
#define DRIVER_WINRING0_KEYBOARD_BACKLIGHT_BRIGHT   0x80

//-------------------------------------------------------------------------   
//  read control port and wait for set/clear of a status bit   
//-------------------------------------------------------------------------   
int
waitportstatus(int bits, int onoff, int timeout)
{
    int ok = FALSE,
        port = EC_CTRLPORT,
        time = 0,
        tick = 10;

    //   
    // wait until input on control port has desired state or times out   
    //   
    for (time = 0; time < timeout; time += tick) {

        BYTE data = ReadIoPortByte(port);

        // check for desired result   
        int flagstate = (((BYTE)data) & bits) != 0,
            wantedstate = onoff != 0;

        if (flagstate == wantedstate) {
            ok = TRUE;
            break;
        }

        // try again after a moment   
        Sleep(tick);
    }

    return ok;
}


//-------------------------------------------------------------------------   
//  write a character to an io port through WinIO device   
//-------------------------------------------------------------------------   
int
writeport(int port, BYTE data)
{
    // write byte via WINIO.SYS   
    WriteIoPortByte(port, data);

    return 1;
}


//-------------------------------------------------------------------------   
//  read a character from an io port through WinIO device   
//-------------------------------------------------------------------------   
int
readport(int port, BYTE* pdata)
{
    // read byte via WINIO.SYS   
    DWORD data = ReadIoPortByte(port);
    *pdata = data;

    return 1;
}


//-------------------------------------------------------------------------   
//  read a byte from the embedded controller (EC) via port io    
//-------------------------------------------------------------------------   
int
ReadByteFromEC(BYTE offset, BYTE* pdata)
{
    int ok;

    // wait for IBF and OBF to clear   
    ok = waitportstatus(EC_STAT_IBF | EC_STAT_OBF, FALSE, DEFAULT_WAIT_TIMEOUT);
    if (ok) {

        // tell 'em we want to "READ"   
        ok = writeport(EC_CTRLPORT, EC_CTRLPORT_READ);
        if (ok) {

            // wait for IBF to clear (command byte removed from EC's input queue)   
            ok = waitportstatus(EC_STAT_IBF, FALSE, DEFAULT_WAIT_TIMEOUT);
            if (ok) {

                // tell 'em where we want to read from   
                ok = writeport(EC_DATAPORT, offset);
                if (ok) {

                    // wait for IBF to clear (address byte removed from EC's input queue)   
                    // Note: Techically we should waitportstatus(OBF,TRUE) here,(a byte being    
                    //       in the EC's output buffer being ready to read).  For some reason   
                    //       this never seems to happen   
                    ok = waitportstatus(EC_STAT_IBF, FALSE, DEFAULT_WAIT_TIMEOUT);
                    if (ok) {
                        BYTE data = -1;

                        // read result (EC byte at offset)   
                        ok = readport(EC_DATAPORT, &data);
                        if (ok)
                            *pdata = data;
                    }
                }
            }
        }
    }

    return ok;
}



//-------------------------------------------------------------------------   
//  write a byte to the embedded controller (EC) via port io   
//-------------------------------------------------------------------------   
int
WriteByteToEC(BYTE offset, BYTE data)
{
    int ok;

    // wait for IBF and OBF to clear   
    ok = waitportstatus(EC_STAT_IBF | EC_STAT_OBF, FALSE, DEFAULT_WAIT_TIMEOUT);
    if (ok) {

        // tell 'em we want to "WRITE"   
        ok = writeport(EC_CTRLPORT, EC_CTRLPORT_WRITE);
        if (ok) {

            // wait for IBF to clear (command byte removed from EC's input queue)   
            ok = waitportstatus(EC_STAT_IBF, FALSE, DEFAULT_WAIT_TIMEOUT);
            if (ok) {

                // tell 'em where we want to write to   
                ok = writeport(EC_DATAPORT, offset);
                if (ok) {

                    // wait for IBF to clear (address byte removed from EC's input queue)   
                    ok = waitportstatus(EC_STAT_IBF, FALSE, DEFAULT_WAIT_TIMEOUT);
                    if (ok) {
                        // tell 'em what we want to write there   
                        ok = writeport(EC_DATAPORT, data);
                        if (ok) {
                            // wait for IBF to clear (data byte removed from EC's input queue)   
                            ok = waitportstatus(EC_STAT_IBF, FALSE, DEFAULT_WAIT_TIMEOUT);
                        }
                    }
                }
            }
        }
    }

    return ok;
}

BYTE GetKeyboardBacklight()
{
    BYTE status = 0, value = 0;
    ReadByteFromEC(
        DRIVER_WINRING0_KEYBOARD_BACKLIGHT_OFFSET,
        &value
    );
    if (value == DRIVER_WINRING0_KEYBOARD_BACKLIGHT_BRIGHT)
    {
        status = KEYBOARD_BACKLIGHT_BRIGHT;
    }
    else if (value == DRIVER_WINRING0_KEYBOARD_BACKLIGHT_DIM)
    {
        status = KEYBOARD_BACKLIGHT_DIM;
    }
    else
    {
        status = KEYBOARD_BACKLIGHT_DISABLED;
    }
    return status;
}

VOID SetKeyboardBacklight(BYTE status)
{
    BYTE value = 0;
    if (status == KEYBOARD_BACKLIGHT_BRIGHT)
    {
        value = DRIVER_WINRING0_KEYBOARD_BACKLIGHT_BRIGHT;
    }
    else if (status == KEYBOARD_BACKLIGHT_DIM)
    {
        value = DRIVER_WINRING0_KEYBOARD_BACKLIGHT_DIM;
    }
    else
    {
        value = DRIVER_WINRING0_KEYBOARD_BACKLIGHT_DISABLED;
    }
    WriteByteToEC(
        DRIVER_WINRING0_KEYBOARD_BACKLIGHT_OFFSET,
        value
    );
}

DWORD Initialize()
{
    hWinRing0 = LoadLibrary(L"WinRing0x64.dll");
    DWORD dwRet = GetLastError();
    if (!hWinRing0)
    {
        return 1;
    }
    InitializeOls = (BOOL(*)())GetProcAddress(hWinRing0, "InitializeOls");
    if (!InitializeOls)
    {
        return 2;
    }
    GetDllStatus = (DWORD(*)())GetProcAddress(hWinRing0, "GetDllStatus");
    if (!GetDllStatus)
    {
        return 3;
    }
    DeinitializeOls = (VOID(*)())GetProcAddress(hWinRing0, "DeinitializeOls");
    if (!DeinitializeOls)
    {
        return 4;
    }
    ReadIoPortByte = (BYTE(*)(USHORT))GetProcAddress(hWinRing0, "ReadIoPortByte");
    if (!ReadIoPortByte)
    {
        return 5;
    }
    WriteIoPortByte = (VOID(*)(USHORT, BYTE))GetProcAddress(hWinRing0, "WriteIoPortByte");
    if (!WriteIoPortByte)
    {
        return 6;
    }
    if (!InitializeOls())
    {
        return GetDllStatus();
    }
    return 0;
}

VOID Deinitialize()
{
    DeinitializeOls();
    FreeLibrary(hWinRing0);
    CloseHandle(hWinRing0);
}
