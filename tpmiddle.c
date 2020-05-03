#if !defined(_UNICODE)
#define _UNICODE
#endif

#if !defined(UNICODE)
#define UNICODE
#endif

#include <windows.h>

#define MAX_MIDDLE_CLICK_DURATION 500 // ms

#define VID_DEVICE  0x17EF
#define PID_USB     0x60EE
#define PID_BT      0x60E1

#define ARRAY_COUNT(_a_) (sizeof(_a_) / sizeof((_a_)[0]))

#define MACRO_START do {
#define MACRO_END   } while (FALSE)

#define EXIT() \
    MACRO_START \
    goto Exit; \
    MACRO_END

#define ERR(_expr_) \
    MACRO_START \
    (_expr_); \
    goto Exit; \
    MACRO_END

#define ZeroStructure(_a_) memset(_a_, 0, sizeof(*(_a_)))

typedef enum _STATE
{
    StateMiddleUp,
    StateMiddleDown,
    StateScroll,
    StateXClicked,

} STATE, *PSTATE;

UINT64 c_MaxMiddleClickDuration;
const UINT16 c_UsagePages[] = {0xFF00, 0xFF10, 0xFFA0};

UINT64 g_MiddleDownTime;
STATE g_State;

VOID
SendClick(
    _In_ UINT Button
    )
{
    INPUT input[2];

    ZeroStructure(&input);
    input[0].type = INPUT_MOUSE;
    input[1].type = INPUT_MOUSE;

    if (Button == 3)
    {
        input[0].mi.dwFlags = MOUSEEVENTF_MIDDLEDOWN;
        input[1].mi.dwFlags = MOUSEEVENTF_MIDDLEUP;
    }
    else
    {
        input[0].mi.dwFlags = MOUSEEVENTF_XDOWN;
        input[0].mi.mouseData = Button - 3;
        input[1].mi.dwFlags = MOUSEEVENTF_XUP;
        input[1].mi.mouseData = Button - 3;
    }

    SendInput(ARRAY_COUNT(input), input, sizeof(input[0]));
}

LRESULT
CALLBACK
WindowProc(
    _In_ HWND hwnd,
    _In_ UINT uMsg,
    _In_ WPARAM wParam,
    _In_ LPARAM lParam
    )
{
    RID_DEVICE_INFO device;
    INT8 dx;
    INT8 dy;
    PRAWINPUT raw;
    UINT8 rawBuffer[FIELD_OFFSET(RAWINPUT, data.hid.bRawData) + 9];
    INPUT scrollInput;
    UINT size;
    UINT64 time;

    UNREFERENCED_PARAMETER(hwnd);
    UNREFERENCED_PARAMETER(wParam);

    if (uMsg != WM_INPUT)
    {
        goto OtherMessage;
    }

    raw = (PVOID)rawBuffer;
    size = sizeof(rawBuffer);

    if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, raw, &size, sizeof(RAWINPUTHEADER)) == -1)
    {
        EXIT();
    }

    if (raw->header.dwType != RIM_TYPEHID)
    {
        EXIT();
    }

    if ((raw->data.hid.dwSizeHid < 3) || (raw->data.hid.dwCount != 1))
    {
        EXIT();
    }

    size = sizeof(device);
    device.cbSize = size;

    if (GetRawInputDeviceInfo(raw->header.hDevice, RIDI_DEVICEINFO, &device, &size) == -1)
    {
        EXIT();
    }

    if (device.dwType != RIM_TYPEHID)
    {
        EXIT();
    }

    if ((device.hid.dwVendorId != VID_DEVICE) ||
        ((device.hid.dwProductId != PID_USB) && (device.hid.dwProductId != PID_BT)))
    {
        EXIT();
    }

    if (raw->data.hid.bRawData[0] == 0x15)
    {
        if ((raw->data.hid.bRawData[2] & 4) != 0)
        {
            g_State = StateMiddleDown;
            QueryPerformanceCounter((PLARGE_INTEGER)&g_MiddleDownTime);
        }
        else
        {
            if (g_State == StateMiddleDown)
            {
                QueryPerformanceCounter((PLARGE_INTEGER)&time);

                if (time <= g_MiddleDownTime + c_MaxMiddleClickDuration)
                {
                    SendClick(3);
                }
            }

            g_State = StateMiddleUp;
        }
    }
    else if (raw->data.hid.bRawData[0] == 0x16)
    {
        dx = raw->data.hid.bRawData[1];
        dy = raw->data.hid.bRawData[2];

        if (dy != 0)
        {
            if (g_State != StateXClicked)
            {
                g_State = StateScroll;
            }

            if (device.hid.dwProductId == PID_USB)
            {
                ZeroStructure(&scrollInput);
                scrollInput.type = INPUT_MOUSE;
                scrollInput.mi.dwFlags = MOUSEEVENTF_WHEEL;
                scrollInput.mi.mouseData = dy * WHEEL_DELTA;
                SendInput(1, &scrollInput, sizeof(scrollInput));
            }
        }
        else if (dx != 0)
        {
            if (g_State == StateMiddleDown)
            {
                g_State = StateXClicked;

                SendClick(((dx < 0) ? 4 : 5));
            }
            else if (g_State != StateXClicked)
            {
                g_State = StateScroll;

                ZeroStructure(&scrollInput);
                scrollInput.type = INPUT_MOUSE;
                scrollInput.mi.dwFlags = MOUSEEVENTF_HWHEEL;
                scrollInput.mi.mouseData = dx * WHEEL_DELTA;
                SendInput(1, &scrollInput, sizeof(scrollInput));
            }
        }
    }

OtherMessage:

    switch (uMsg)
    {
    case WM_NCCREATE:
        return TRUE;
    }

Exit:

    return 0;
}

VOID
Main(
    VOID
    )
{
    RAWINPUTDEVICE devices[ARRAY_COUNT(c_UsagePages)];
    BOOLEAN devicesRegistered;
    UINT error;
    UINT i;
    MSG message;
    BOOL messageStatus;
    UINT64 qpcFrequency;
    HWND window;
    WNDCLASSEX windowClass = {0};
    ATOM windowClassAtom;

    error = 0;
    devicesRegistered = FALSE;
    window = NULL;
    windowClassAtom = 0;

    if (!SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS))
    {
        ERR(error = GetLastError());
    }

    QueryPerformanceFrequency((PLARGE_INTEGER)&qpcFrequency);
    c_MaxMiddleClickDuration = (UINT64)MAX_MIDDLE_CLICK_DURATION * qpcFrequency / 1000;

    ZeroStructure(&windowClass);
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = GetModuleHandle(NULL);
    windowClass.lpszClassName = L"MessageWindowClass";

    windowClassAtom = RegisterClassEx(&windowClass);
    if (windowClassAtom == 0)
    {
        ERR(error = GetLastError());
    }

    window = CreateWindowEx(0, MAKEINTATOM(windowClassAtom), NULL, 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, NULL, NULL);
    if (window == NULL)
    {
        ERR(error = GetLastError());
    }

    for (i = 0; i < ARRAY_COUNT(devices); i++)
    {
        devices[i].usUsagePage = c_UsagePages[i];
        devices[i].usUsage = 1;
        devices[i].dwFlags = RIDEV_INPUTSINK;
        devices[i].hwndTarget = window;
    }

    if (!RegisterRawInputDevices(devices, ARRAY_COUNT(devices), sizeof(devices[0])))
    {
        ERR(error = GetLastError());
    }

    devicesRegistered = TRUE;

    for (;;)
    {
        messageStatus = GetMessage(&message, NULL, 0, 0);

        if (messageStatus == 0)
        {
            ERR(error = (UINT)message.wParam);
        }
        else if (messageStatus == -1)
        {
            ERR(error = GetLastError());
        }

        TranslateMessage(&message);
        DispatchMessage(&message);
    }

Exit:

    if (devicesRegistered)
    {
        for (i = 0; i < ARRAY_COUNT(devices); i++)
        {
            devices[i].dwFlags = RIDEV_REMOVE;
            devices[i].hwndTarget = NULL;
        }

        RegisterRawInputDevices(devices, ARRAY_COUNT(devices), sizeof(devices[0]));
    }

    if (window != NULL)
    {
        DestroyWindow(window);
    }

    if (windowClassAtom != 0)
    {
        UnregisterClass(MAKEINTATOM(windowClassAtom), windowClass.hInstance);
    }

    ExitProcess(error);
}
