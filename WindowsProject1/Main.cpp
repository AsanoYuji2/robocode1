
#include <windows.h>
#include <windowsx.h>

#include "BlockManager.h"
#include "JavaGenerator.h"

BlockManager g_manager;

LRESULT CALLBACK WndProc(
    HWND hwnd,
    UINT msg,
    WPARAM wp,
    LPARAM lp)
{
    int x = GET_X_LPARAM(lp);
    int y = GET_Y_LPARAM(lp);

    switch (msg)
    {
    case WM_CREATE:
        g_manager.Initialize();
        return 0;

    case WM_LBUTTONDOWN:
        if (x >= 10 && x <= 140 && y >= 10 && y <= 48)
        {
            if (ExportJava(g_manager.GetBlocks()))
            {
                MessageBoxW(
                    hwnd,
                    L"Java output completed.",
                    L"Java output",
                    MB_OK | MB_ICONINFORMATION);
            }
            else
            {
                MessageBoxW( hwnd,
                    L"Failed to create the fire",
                    L"Error",
                    MB_OK | MB_ICONERROR);
            }
        }
        else
        {
            g_manager.OnMouseDown(hwnd, x, y);
        }

        InvalidateRect(hwnd, nullptr, TRUE);
        return 0;

    case WM_MOUSEMOVE:
        g_manager.OnMouseMove(x, y);
        InvalidateRect(hwnd, nullptr, TRUE);
        return 0;

    case WM_LBUTTONUP:
        g_manager.OnMouseUp(hwnd);
        InvalidateRect(hwnd, nullptr, TRUE);
        return 0;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        RECT client;
        GetClientRect(hwnd, &client);

        FillRect(
            hdc,
            &client,
            (HBRUSH)GetStockObject(WHITE_BRUSH));

        RECT button = { 10, 10, 140, 48 };

        HBRUSH br = CreateSolidBrush(RGB(45, 45, 55));
        FillRect(hdc, &button, br);
        DeleteObject(br);

        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(255, 255, 255));

        DrawTextW(
            hdc,
            L"Java output",
            -1,
            &button,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        g_manager.DrawAll(hdc);

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wp, lp);
}

int WINAPI wWinMain(
    HINSTANCE instance,
    HINSTANCE,
    PWSTR,
    int show)
{
    const wchar_t CLASS_NAME[] = L"MyBlockEditor";

    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = instance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground =
        (HBRUSH)GetStockObject(WHITE_BRUSH);

    if (!RegisterClassW(&wc))
        return 0;

    HWND hwnd = CreateWindowExW(
        0,
        CLASS_NAME,
        L"自作ブロックエディタ",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        900,
        650,
        nullptr,
        nullptr,
        instance,
        nullptr);

    if (!hwnd)
        return 0;

    ShowWindow(hwnd, show);
    UpdateWindow(hwnd);

    MSG msg = {};

    while (GetMessageW(&msg, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return (int)msg.wParam;
}
