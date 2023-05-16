#include <windows.h>
#include <winioctl.h>
#include <stdio.h>
#include <winternl.h>

int DriverCommunicate(BOOL FNState, BOOL debug)
{
    HANDLE ATKACPI = CreateFileW(L"\\\\.\\ATKACPI", 0xc0000000, 3, (LPSECURITY_ATTRIBUTES)0x0, 3, 0, (HANDLE)0x0);
    if (ATKACPI == 0x0 || ATKACPI == INVALID_HANDLE_VALUE) {
        printf("Failed to find ATKACPI handle.\n");
        return -1;
    }

    enum FN_SWITCH_STATE {
        FN_SWITCH_OFF = 0x0,
        FN_SWITCH_ON = 0x1,
    };
    // Special thanks to Daft Punk and WinDBG
    int lpDataIn[0x10] = { 0 };
    ////lpDataIn[0] = 0x53545344; // Equals "STSD". Probably authentication?
    // Magic numbers extracted from debugging stack memory (edi register)
    lpDataIn[0] = 0x53564544; // Equals "SVED". Probably authentication?
    lpDataIn[1] = 0x8; // Signifying FN switch?
    lpDataIn[2] = 0x100023;
    lpDataIn[3] = FNState == TRUE ? FN_SWITCH_ON : FN_SWITCH_OFF; // Message to driver

    // Output data from the driver
    DWORD OutBuffer[0x400u] = { 0 };
    DWORD BytesReturned = 0;

    BOOL Err = DeviceIoControl(ATKACPI, 0x22240Cu, lpDataIn, 0x10, OutBuffer, 0x400u, &BytesReturned, 0);
    if (Err == FALSE) {
        printf("DeviceIoControl returned false\n");
    } else if (debug == TRUE) {
        printf("DeviceIoControl returned true\n%d bytes returned\n", BytesReturned);
        printf("Data returned in hex:\n");
        for (unsigned int i = 0; i < BytesReturned; i++) {
            printf("%x ", OutBuffer[i]);
        }
    }
    // Remember to cleanup :O
    CloseHandle(ATKACPI);
    return 0;
}

// Not sure exactly what this function does, but it is executed everytime AsKeybdHk.exe is run.
int TestFNSwitchExists()
{
    HANDLE ATKACPI = CreateFileW(L"\\\\.\\ATKACPI", 0xc0000000, 3, (LPSECURITY_ATTRIBUTES)0x0, 3, 0, (HANDLE)0x0);
    if (ATKACPI == 0x0 || ATKACPI == INVALID_HANDLE_VALUE) {
        printf("Failed to find ATKACPI handle.");
        return -1;
    }

    int lpDataIn[0xc] = { 0 };
    lpDataIn[0] = 0x53545344; // Equals "STSD"
    lpDataIn[1] = 0x4;
    lpDataIn[2] = 0x100023;
    //lpDataIn[3] = 0xabababab; // Honestly probably just a marker for uninitialized memory

    DWORD OutBuffer[0x400u] = { 0 };
    DWORD BytesReturned = 0;

    BOOL CommSuccess = DeviceIoControl(ATKACPI, 0x22240c, lpDataIn, 0xc, &OutBuffer, 0x400, &BytesReturned, (LPOVERLAPPED)0x0);
    if (CommSuccess == FALSE) {
        printf("You messed up here.\n Error code %d\n", GetLastError());
    } else {
        printf("Communication success. Returned %d bytes:\n", BytesReturned);
        // using min just in case the driver does anything funny
        for (unsigned int i = 0; i < min(BytesReturned, 0x400); i++) {
            printf("%x ", OutBuffer[i]);
        }
    }

    CloseHandle(ATKACPI);

    return 0;
}

// These are functions I've used to test out some findings.
// I'm leaving them here just in case they're useful to someone
/*int PostMagicMessage()
{
    // https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-registerwindowmessagew
    UINT SwitchMessage = RegisterWindowMessageW(L"Transparent Button Click Message");
    if (SwitchMessage == 0) {
        printf("Error registering message\n");
        printf("code %d", GetLastError());
        return -1;
    }
    //PostMessageW(0x005E0450, 0xC215, 0x8080, 0x0);
    // AsKeybdHk.exe window handle I copied from Spy++
    // 0x8080 for Hotkeys, 0x8081 for F1-12
    PostMessageW((HWND)0x005E0450, SwitchMessage, 0x8080, 0x0);

    // The broadcast message does work, but is unnecessary
    //PostMessage(HWND_BROADCAST, 0xC215, 0x8080, 0x0);

    return 0;
}
int HControlMagicMessage()
{
    UINT SwitchMessage = RegisterWindowMessageW(L"ATKConfig Application Notification to ATKHotkey");
    HWND hWnd = FindWindowW(L"HCONTROL", L"HControl"); // As specified by Asus' software
    if (hWnd != (HWND)0x0) {
        int param = TRUE ? 0 : 1;
        //PostMessageW(hWnd, SwitchMessage, 4, 0);
        PostMessageW(hWnd, SwitchMessage, 4, 1);
    }
    return 0;
}*/

void PrintHelp(LPWSTR firstarg)
{
    // Capital S for wide string
    printf("Usage: %S [debug] {on|off}", firstarg);
}

int main()
{
    BOOL state = -1;
    BOOL debug = FALSE;
    BOOL err = FALSE;
    LPWSTR cmdLine = GetCommandLineW();
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(cmdLine, &argc);
    // argv should contain only one or two options
    if (argc < 2 || argc > 3) {
        PrintHelp(argv[0]);
    } else {
        for (int i = 1; i < argc; i++) {
            if (lstrcmpW(argv[i], L"on") == 0) {
                if (state != -1) err = TRUE;
                state = TRUE;
            } else if (lstrcmpW(argv[i], L"off") == 0) {
                if (state != -1) err = TRUE;
                state = FALSE;
            } else if (lstrcmpW(argv[i], L"debug") == 0) {
                if (debug != FALSE) err = TRUE;
                debug = TRUE;
            } else {
                err = TRUE;
            }
        }
        // We didn't get a state somehow
        if (state == -1) err = TRUE;

        if (err == TRUE) {
            PrintHelp(argv[0]);
        } else {
            DriverCommunicate(state, debug);
        }
    }
    return 0;
}