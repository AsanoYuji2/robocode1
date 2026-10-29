int WINAPI WinMain(
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPSTR lpCmdLine,
    int nCmdShow)
{
    (void)hPrevInstance;
    (void)lpCmdLine;

    // GDI+ の初期化
    GdiplusStartupInput gdiplusStartupInput;
    GdiplusStartup(&g_gdiplusToken, &gdiplusStartupInput, nullptr);

    // ウィンドウクラスの登録
    const wchar_t CLASS_NAME[] = L"MyBlockEditor";

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    wc.hbrBackground =
        reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = CLASS_NAME;

    if (!RegisterClassExW(&wc))
    {
        MessageBoxW(
            nullptr,
            L"ウィンドウクラスの登録に失敗しました。",
            L"エラー",
            MB_OK | MB_ICONERROR
        );

        return 1;
    }

    // メインウィンドウの作成
    HWND hwnd = CreateWindowExW(
        WS_EX_COMPOSITED,
        CLASS_NAME,
        L"自作ブロックエディタ",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        1000,
        700,
        nullptr,
        nullptr,
        hInstance,
        nullptr
    );

    if (!hwnd)
    {
        MessageBoxW(
            nullptr,
            L"ウィンドウの作成に失敗しました。",
            L"エラー",
            MB_OK | MB_ICONERROR
        );

        return 1;
    }

    ShowWindow(hwnd, SW_MAXIMIZE);
    UpdateWindow(hwnd);
    ACCEL accel[] =
    {
        { FVIRTKEY | FCONTROL, 'A', ID_SHORTCUT_SELECT_ALL },
        { FVIRTKEY | FCONTROL, 'S', ID_SHORTCUT_SAVE },
        { FVIRTKEY | FCONTROL, 'O', ID_SHORTCUT_LOAD },
        { FVIRTKEY | FCONTROL, 'Z', ID_SHORTCUT_UNDO },
        { FVIRTKEY | FCONTROL, 'Y', ID_SHORTCUT_REDO },
        { FVIRTKEY | FCONTROL, 'C', ID_SHORTCUT_COPY },
        { FVIRTKEY | FCONTROL, 'V', ID_SHORTCUT_PASTE },
        { FVIRTKEY, VK_DELETE, ID_SHORTCUT_DELETE }
    };

    g_hAccel = CreateAcceleratorTableW(
        accel,
        static_cast<int>(sizeof(accel) / sizeof(accel[0]))
    );

    if (!g_hAccel)
    {
        MessageBoxW(
            hwnd,
            L"ショートカットの登録に失敗しました。",
            L"エラー",
            MB_OK | MB_ICONERROR
        );
    }

    // メッセージループ
    MSG msg{};

    while (GetMessageW(&msg, nullptr, 0, 0) > 0)
    {
        if (g_hAccel &&
            TranslateAcceleratorW(hwnd, g_hAccel, &msg))
        {
            continue;
        }

        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    // GDI+ の終了処理
    GdiplusShutdown(g_gdiplusToken);

    return static_cast<int>(msg.wParam);
}