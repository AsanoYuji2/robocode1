
LRESULT CALLBACK DragPreviewProc(
    HWND hwnd,
    UINT msg,
    WPARAM wParam,
    LPARAM lParam)
{
    switch (msg)
    {
    case WM_NCHITTEST:
        // マウス操作をプレビューウィンドウが横取りしない
        return HTTRANSPARENT;

    case WM_MOUSEACTIVATE:
        return MA_NOACTIVATE;

    case WM_ERASEBKGND:
        return TRUE;

    case WM_PAINT:
    {
        PAINTSTRUCT ps{};
        HDC hdc = BeginPaint(hwnd, &ps);

        RECT rc{};
        GetClientRect(hwnd, &rc);

        HBRUSH brush = CreateSolidBrush(
            CATEGORY_COLORS[
                static_cast<int>(g_selectedCategory)
            ]);

        FillRect(hdc, &rc, brush);
        DeleteObject(brush);

        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(255, 255, 255));

        std::wstring name = g_dragBlockFunctionName.empty()
            ? GetBlockName(g_dragBlockType)
            : L"自作関数： " + g_dragBlockFunctionName;

        DrawTextW(
            hdc,
            name.c_str(),
            -1,
            &rc,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE
        );

        EndPaint(hwnd, &ps);
        return 0;
    }
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK WndProc(
    HWND hwnd,
    UINT msg,
    WPARAM wParam,
    LPARAM lParam)
{
    switch (msg)
    {
    case WM_ERASEBKGND:
        // 背景の自動消去を抑制してちらつきを防ぐ
        return TRUE;

    case WM_CREATE:
    {
        g_mainWindow = hwnd;

        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = DragPreviewProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = L"BlockDragPreviewWindow";
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

        RegisterClassExW(&wc);

        g_dragPreviewWnd = CreateWindowExW(
            WS_EX_TOPMOST | WS_EX_TOOLWINDOW |
            WS_EX_TRANSPARENT | WS_EX_NOACTIVATE,
            L"BlockDragPreviewWindow",
            L"",
            WS_POPUP,
            0, 0, 168, 30,
            hwnd,
            nullptr,
            GetModuleHandleW(nullptr),
            nullptr
        );

        if (g_dragPreviewWnd)
            ShowWindow(g_dragPreviewWnd, SW_HIDE);


        g_manager.Initialize();

        // メインではイベントブロックを使用できる
        g_manager.SetEventBlocksAllowed(true);

        // 関数編集用の管理役は空の状態から始める
        g_functionManager.ReplaceBlocks({}, {});

        // 自作関数の中ではイベントブロックを使用できない
        g_functionManager.SetEventBlocksAllowed(false);

        ReloadUserFunctions();
        UpdateView();


        // ==========================================
        // 左サイドバー上部に「メイン」「関数」タブを作成
        // ==========================================
        g_tabMainBtn = CreateWindowW(
            L"BUTTON", L"メイン",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            12, 55, 100, 30,
            hwnd, reinterpret_cast<HMENU>(ID_TAB_MAIN),
            GetModuleHandleW(nullptr), nullptr
        );

        g_tabFuncBtn = CreateWindowW(
            L"BUTTON", L"関数",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            114, 55, 100, 30,
            hwnd, reinterpret_cast<HMENU>(ID_TAB_FUNCTION),
            GetModuleHandleW(nullptr), nullptr
        );

        // ==========================================
        //作成した関数一覧用のリストボックスを作成（初期は非表示）
        // ==========================================
        g_createdFuncList = CreateWindowW(
            L"LISTBOX", nullptr,
            WS_CHILD | WS_BORDER | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT,
            12, 95, 202, 500,
            hwnd, reinterpret_cast<HMENU>(ID_CREATED_FUNC_LIST),
            GetModuleHandleW(nullptr), nullptr
        );
        ShowWindow(g_createdFuncList, SW_HIDE);

        g_functionHelpLabel = CreateWindowW(
            L"STATIC",
            L"左のカテゴリからブロックを選び、一覧からキャンバスへドラッグして組み立てます。",
            WS_CHILD | SS_LEFT,
            12, 600, 220, 70,
            hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
        ShowWindow(g_functionHelpLabel, SW_SHOW);

        // 関数を保存したファイルを一覧に読み込む
        ReloadUserFunctions();

        // 左側のカテゴリボタン
        CreateCategoryButtons(hwnd);

        // ブロック一覧
        g_blockList = CreateWindowW(
            L"LISTBOX",
            nullptr,
            WS_CHILD | // SW_VISIBLE を外す
            WS_BORDER | WS_VSCROLL |
            LBS_NOTIFY | LBS_NOINTEGRALHEIGHT |
            LBS_OWNERDRAWFIXED | LBS_HASSTRINGS,
            150, 60, 180, 190,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_BLOCK_LIST)),
            GetModuleHandleW(nullptr),
            nullptr
        );
        // 起動時は隠しておく
        ShowWindow(g_blockList, SW_HIDE);

        InitializeBlockList(g_blockList);

        // リストからキャンバスへドラッグ
        g_oldListProc = reinterpret_cast<WNDPROC>(
            SetWindowLongPtrW(
                g_blockList,
                GWLP_WNDPROC,
                reinterpret_cast<LONG_PTR>(BlockListProc)
            )
            );

        // ====================================================
        // 上部ツールバー（画像表示用ボタン生成）
        // ====================================================
        CreateWindowW(L"BUTTON", L"新規", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 250, 9, 65, 32, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_TOOL_NEW)), GetModuleHandleW(nullptr), nullptr);
        CreateWindowW(L"BUTTON", L"開く", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 320, 9, 65, 32, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_TOOL_OPEN)), GetModuleHandleW(nullptr), nullptr);
        CreateWindowW(L"BUTTON", L"保存", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 390, 9, 65, 32, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_TOOL_SAVE)), GetModuleHandleW(nullptr), nullptr);
        CreateWindowW(L"BUTTON", L"元に戻す", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 460, 9, 65, 32, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_TOOL_UNDO)), GetModuleHandleW(nullptr), nullptr);
        CreateWindowW(L"BUTTON", L"やり直し", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 530, 9, 65, 32, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_TOOL_REDO)), GetModuleHandleW(nullptr), nullptr);
        CreateWindowW(L"BUTTON", L"ロボットにする", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 600, 9, 130, 32, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_TOOL_RUN)), GetModuleHandleW(nullptr), nullptr);
        //ロボット名ラベルと入力欄
        CreateWindowW(L"STATIC", L" ロボット名前", WS_CHILD | WS_VISIBLE, 735, 9, 110, 20, hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
        g_editRobotName = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"MyRobot", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 847, 7, 150, 26, hwnd, reinterpret_cast<HMENU>(ID_EDIT_ROBOT_NAME), GetModuleHandleW(nullptr), nullptr);

        //保存先フォルダのパス入力欄と参照ボタン
        CreateWindowW(L"STATIC", L" ロボット保存先", WS_CHILD | WS_VISIBLE, 1000, 9, 125, 20, hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
        g_editFolderPath = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"C:\\robocode\\robots", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 1127, 7, 150, 26, hwnd, reinterpret_cast<HMENU>(ID_EDIT_FOLDER_PATH), GetModuleHandleW(nullptr), nullptr);
        CreateWindowW(L"BUTTON", L" 開く", WS_CHILD | WS_VISIBLE, 1285, 7, 70, 28, hwnd, reinterpret_cast<HMENU>(ID_BTN_BROWSE), GetModuleHandleW(nullptr), nullptr);

        // BMP 画像のロード
        g_hBmpNew = (HBITMAP)LoadImageW(nullptr, L"assets/new.bmp", IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
        g_hBmpOpen = (HBITMAP)LoadImageW(nullptr, L"assets/open.bmp", IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
        g_hBmpSave = (HBITMAP)LoadImageW(nullptr, L"assets/save.bmp", IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
        g_hBmpUndo = (HBITMAP)LoadImageW(nullptr, L"assets/undo.bmp", IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
        g_hBmpRedo = (HBITMAP)LoadImageW(nullptr, L"assets/redo.bmp", IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
        g_hBmpRun = (HBITMAP)LoadImageW(nullptr, L"assets/run.bmp", IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);

        UpdateButtonColors();
        return 0; // ← WM_CREATE の最後に 1 回だけ return 0 します
    }


    // ========================================================
    // ボタン描画
    // ========================================================
    case WM_DRAWITEM:
    {
        const DRAWITEMSTRUCT* dis = reinterpret_cast<const DRAWITEMSTRUCT*>(lParam);
        if (!dis) return TRUE;

        int id = static_cast<int>(dis->CtlID);

        if (id == ID_TAB_MAIN || id == ID_TAB_FUNCTION)
        {
            HDC hdc = dis->hDC;
            RECT rc = dis->rcItem;
            bool isMain = (id == ID_TAB_MAIN);
            bool selected = (isMain && g_activeSidebarTab == 0) || (!isMain && g_activeSidebarTab != 0);

            COLORREF bgCol = selected ? RGB(59, 130, 246) : RGB(229, 231, 235); // 選択時は青、非選択時はグレー
            COLORREF fgCol = selected ? RGB(255, 255, 255) : RGB(75, 85, 99);

            HBRUSH brush = CreateSolidBrush(bgCol);
            FillRect(hdc, &rc, brush);
            DeleteObject(brush);

            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, fgCol);

            const wchar_t* label = isMain ? L"メイン" : L"関数";
            DrawTextW(hdc, label, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            return TRUE;
        }

        if (id >= ID_CATEGORY_BASE && id < ID_CATEGORY_BASE + CATEGORY_COUNT)
        {
            DrawCategoryButton(dis, id - ID_CATEGORY_BASE);
            return TRUE;
        }

        if (id == ID_BLOCK_LIST)
        {


            DrawBlockListItem(dis);
            return TRUE;
        }

        // ====================================================
        // 上部ツールバーの BMP 画像ボタン描画処理
        // ====================================================
        if (id >= ID_TOOL_NEW && id <= ID_TOOL_JAVA)
        {
            HDC hdc = dis->hDC;
            RECT rc = dis->rcItem;

            // 対応する HBITMAP を取得
            HBITMAP hBmp = nullptr;
            if (id == ID_TOOL_NEW) hBmp = g_hBmpNew;
            else if (id == ID_TOOL_OPEN) hBmp = g_hBmpOpen;
            else if (id == ID_TOOL_SAVE) hBmp = g_hBmpSave;
            else if (id == ID_TOOL_UNDO) hBmp = g_hBmpUndo;
            else if (id == ID_TOOL_REDO) hBmp = g_hBmpRedo;
            else if (id == ID_TOOL_RUN)  hBmp = g_hBmpRun;

            if (hBmp)
            {
                HDC memDC = CreateCompatibleDC(hdc);
                HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, hBmp);

                BITMAP bm{};
                GetObject(hBmp, sizeof(BITMAP), &bm);

                // 伸縮の品質を向上（クリアに表示）
                SetStretchBltMode(hdc, HALFTONE);

                // 「ロボットにする」も含めて、すべてのボタンを領域いっぱいに全面描画する
                int width = rc.right - rc.left;
                int height = rc.bottom - rc.top;

                StretchBlt(hdc, rc.left, rc.top, width, height, memDC, 0, 0, bm.bmWidth, bm.bmHeight, SRCCOPY);

                SelectObject(memDC, oldBmp);
                DeleteDC(memDC);
            }
            else
            {
                // 画像がない場合の背景描画
                COLORREF bgColor = RGB(45, 55, 72);
                if (dis->itemState & ODS_SELECTED)
                {
                    bgColor = RGB(GetRValue(bgColor) * 3 / 4, GetGValue(bgColor) * 3 / 4, GetBValue(bgColor) * 3 / 4);
                }
                HBRUSH bgBrush = CreateSolidBrush(bgColor);
                FillRect(hdc, &rc, bgBrush);
                DeleteObject(bgBrush);

                // フォールバック（文字表示）
                SetBkMode(hdc, TRANSPARENT);
                SetTextColor(hdc, RGB(255, 255, 255));
                const wchar_t* fallbackText = L"";
                if (id == ID_TOOL_NEW) fallbackText = L"新規";
                else if (id == ID_TOOL_OPEN) fallbackText = L"開く";
                else if (id == ID_TOOL_SAVE) fallbackText = L"保存";
                else if (id == ID_TOOL_UNDO) fallbackText = L"戻す";
                else if (id == ID_TOOL_REDO) fallbackText = L"やり直し";
                else if (id == ID_TOOL_RUN)  fallbackText = L"ロボットにする";

                DrawTextW(hdc, fallbackText, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            }

            return TRUE;
        }

        return TRUE;
    }

    // ========================================================
    // キーボードショートカット
    // ========================================================

    case WM_KEYDOWN:
    {
        // Escキー：選択・範囲選択・ドラッグを解除
        if (wParam == VK_ESCAPE)
        {
            ActiveManager().ClearSelection();
            ActiveManager().EndRangeSelection();

            if (GetCapture() == hwnd)
                ReleaseCapture();

            InvalidateEditor();
            return 0;
        }

        // Deleteキー
        if (wParam == VK_DELETE)
        {
            if (ActiveManager().DeleteSelectedBlock())
                InvalidateEditor();

            return 0;
        }

        bool ctrlPressed =
            (GetKeyState(VK_CONTROL) & 0x8000) != 0;

        if (ctrlPressed)
        {
            switch (wParam)
            {
            case 'A':
                ActiveManager().SelectAllBlocks();
                InvalidateEditor();
                return 0;

            case 'S':
                SaveProject(hwnd);
                return 0;

            case 'O':
                LoadProject(hwnd);
                InvalidateEditor();
                return 0;

            case 'Z':
                if (ActiveManager().Undo())
                    InvalidateEditor();
                return 0;

            case 'Y':
                if (ActiveManager().Redo())
                    InvalidateEditor();
                return 0;

            case 'C':
                ActiveManager().CopySelectedBlock();
                return 0;

            case 'X':
            {
                // 選択ブロックをコピーしてから削除
                if (ActiveManager().CopySelectedBlock())
                {
                    if (ActiveManager().DeleteSelectedBlock())
                    {
                        InvalidateEditor();
                    }
                }
                return 0;
            }

            case 'V':
            {
                const auto& blocks = ActiveManager().GetBlocks();
                int index = ActiveManager().GetSelectedBlockIndex();

                int x = 0;
                int y = 0;

                // ブロックが選択されている場合は、選択ブロックの右下に貼り付ける
                if (index >= 0 &&
                    index < static_cast<int>(blocks.size()))
                {
                    x = blocks[index].x + 30;
                    y = blocks[index].y + 30;
                }
                else
                {
                    // 選択されていない場合はマウスカーソル位置に貼り付ける
                    POINT cursor;

                    if (!GetCursorPos(&cursor))
                        return 0;

                    // 画面座標からウィンドウ内の座標に変換
                    ScreenToClient(hwnd, &cursor);

                    // キャンバス外なら貼り付けない
                    if (!IsCanvasPoint(cursor))
                        return 0;

                    // ウィンドウ内の座標からワールド座標に変換
                    POINT world = ActiveManager().ScreenToWorld(
                        cursor.x,
                        cursor.y
                    );

                    x = world.x;
                    y = world.y;
                }

                if (ActiveManager().PasteCopiedBlock(x, y))
                    InvalidateEditor();

                return 0;
            }
            }
        }

        break;
    }

    // ========================================================
    // ボタンクリック
    // ========================================================

    case WM_COMMAND:
    {
        int id = LOWORD(wParam);
        int notification = HIWORD(wParam);

        // ==========================================
        // 「メイン」タブが押されたとき
        // ==========================================
        if (id == ID_TAB_MAIN && notification == BN_CLICKED)
        {
            if (g_isEditingFunction && !SaveCurrentFunction())
                return 0;

            g_isEditingFunction = false;
            g_currentFunctionName.clear();
            g_activeSidebarTab = 0;
            CloseBlockPalette();

            for (int i = 0; i < CATEGORY_COUNT; ++i)
                if (g_categoryButtons[i]) ShowWindow(g_categoryButtons[i], SW_SHOW);
            if (g_createdFuncList) ShowWindow(g_createdFuncList, SW_HIDE);
            SetFunctionHelp(L"左のカテゴリからブロックを選び、一覧からキャンバスへドラッグして組み立てます。", true);

            UpdateView();
            UpdateButtonColors();
            InvalidateEditor();
            return 0;
        }

        // ==========================================
        // 「関数」タブが押されたとき
        // ==========================================
        if (id == ID_TAB_FUNCTION && notification == BN_CLICKED)
        {
            // 関数タブは「関数一覧へ戻る」ボタンとしても働く。
            ShowFunctionList();
            return 0;
        }

        // 関数一覧：先頭をクリックすると新規作成、既存項目はダブルクリックで編集。
        if (id == ID_CREATED_FUNC_LIST)
        {
            const LRESULT selectedIndex = SendMessageW(g_createdFuncList, LB_GETCURSEL, 0, 0);
            if (notification == LBN_SELCHANGE && selectedIndex == 0)
            {
                if (!CreateUserFunction(hwnd) && g_createdFuncList)
                    SendMessageW(g_createdFuncList, LB_SETCURSEL, static_cast<WPARAM>(-1), 0);
                return 0;
            }
            if (notification == LBN_DBLCLK && selectedIndex > 0 &&
                selectedIndex <= static_cast<LRESULT>(g_userFunctions.size()))
            {
                const std::wstring name = g_userFunctions[static_cast<size_t>(selectedIndex - 1)].name;
                OpenUserFunctionEditor(name);
                return 0;
            }
            return 0;
        }

        // 「参照...」ボタンが押されたとき
        if (id == ID_BTN_BROWSE && notification == BN_CLICKED)
        {
            IFileDialog* pFileDialog = nullptr;
            HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pFileDialog));

            if (SUCCEEDED(hr))
            {
                //フォルダ選択モードにするためのフラグを設定
                DWORD dwOptions;
                if (SUCCEEDED(pFileDialog->GetOptions(&dwOptions)))
                {
                    pFileDialog->SetOptions(dwOptions | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
                }

                //最初に開くフォルダを C:\robocode\robots に強制指定
                IShellItem* pDefaultFolder = nullptr;
                std::wstring initialPath = L"C:\\robocode\\robots";

                // フォルダが存在しない場合に備えて自動作成しておく
                std::filesystem::create_directories(initialPath);

                if (SUCCEEDED(SHCreateItemFromParsingName(initialPath.c_str(), NULL, IID_PPV_ARGS(&pDefaultFolder))))
                {
                    pFileDialog->SetFolder(pDefaultFolder);
                    pDefaultFolder->Release();
                }

                // ダイアログのタイトルを設定
                pFileDialog->SetTitle(L"ロボットの保存先フォルダ（パッケージ）を選択してください");

                //ダイアログを表示
                if (SUCCEEDED(pFileDialog->Show(hwnd)))
                {
                    IShellItem* pResult = nullptr;
                    if (SUCCEEDED(pFileDialog->GetResult(&pResult)))
                    {
                        PWSTR pszPath = nullptr;
                        if (SUCCEEDED(pResult->GetDisplayName(SIGDN_FILESYSPATH, &pszPath)))
                        {
                            // 選択されたフォルダのパスをテキストボックスにセット
                            SetWindowTextW(g_editFolderPath, pszPath);
                            CoTaskMemFree(pszPath);
                        }
                        pResult->Release();
                    }
                }
                pFileDialog->Release();
            }
            return 0;
        }

        if (id == 1090 && notification == EN_KILLFOCUS)
        {
            FinishNumericEdit(hwnd, true);
            return 0;
        }

        // カテゴリ切り替え
        if (id >= ID_CATEGORY_BASE &&
            id < ID_CATEGORY_BASE + CATEGORY_COUNT)
        {
            if (notification == BN_CLICKED)
            {
                SelectCategory(
                    static_cast<BlockCategory>(
                        id - ID_CATEGORY_BASE
                        )
                );
            }

            return 0;
        }

        switch (id)
        {

            // 新規作成
        case ID_TOOL_NEW:
            if (notification == BN_CLICKED)
            {
                int result = MessageBoxW(
                    hwnd,
                    L"現在のブロックをすべて消去して、新規作成しますか？\n"
                    L"必要な場合は、先に保存してください。",
                    L"新規作成",
                    MB_YESNO | MB_ICONWARNING
                );


                if (result == IDYES)
                {
                    // 1. 編集中の関数データやメインのブロックをクリア
                    if (g_isEditingFunction)
                    {
                        g_functionManager.ReplaceBlocks({}, {});
                    }
                    else
                    {
                        g_manager.ReplaceBlocks({}, {});
                    }

                    // 2. 現在のプロジェクトで使用する関数一覧だけをクリア
                    // UserFunctions内の保存済みファイルは削除しない
                    g_userFunctions.clear();

                    // 3. 関数編集状態を解除
                    g_isEditingFunction = false;
                    g_currentFunctionName.clear();
                    g_activeSidebarTab = 0;

                    // 4. 関数一覧の表示を更新
                    RefreshFunctionList();

                    // 5. シフトや位置バックアップをリセット
                    g_originalXMap.clear();
                    g_blockShiftMap.clear();

                    // 6. ブロック一覧パレットを閉じる
                    if (g_showBlockList)
                    {
                        CloseBlockPalette();
                    }

                    // 7. 表示位置やズームを初期値に戻す
                    g_zoom = INITIAL_ZOOM;
                    g_viewX = INITIAL_VIEW_X;
                    g_viewY = INITIAL_VIEW_Y;
                    g_canvasLeft = 230;

                    UpdateView();
                    InvalidateEditor();

                    MessageBoxW(
                        hwnd,
                        L"新しいプロジェクトを作成しました。",
                        L"新規作成完了",
                        MB_OK | MB_ICONINFORMATION
                    );
                }

            }
            return 0;

            // 開く
        case ID_TOOL_OPEN:
            if (notification == BN_CLICKED)
            {
                LoadProject(hwnd);
                InvalidateEditor();
            }
            return 0;

            // 保存
        case ID_TOOL_SAVE:
            if (notification == BN_CLICKED)
            {
                SaveProject(hwnd);
            }
            return 0;

            // 元に戻す
        case ID_TOOL_UNDO:
            if (notification == BN_CLICKED)
            {
                if (ActiveManager().Undo())
                    InvalidateEditor();
            }
            return 0;

            // やり直し
        case ID_TOOL_REDO:
            if (notification == BN_CLICKED)
            {
                if (ActiveManager().Redo())
                    InvalidateEditor();
            }
            return 0;

            // 実行
        case ID_TOOL_RUN:
            if (notification == BN_CLICKED)
            {
                if (g_isEditingFunction)
                {
                    MessageBoxW(hwnd, L"ロボットとして実行するには、先に「メイン」タブへ戻ってください。",
                        L"実行", MB_OK | MB_ICONINFORMATION);
                    return 0;
                }
                const auto& blocks = g_manager.GetBlocks();
                if (blocks.empty())
                {
                    MessageBoxW(hwnd, L"ブロックが配置されていません。", L"警告", MB_OK | MB_ICONWARNING);
                    return 0;
                }

                //画面上の入力欄からテキストを取得
                wchar_t robotName[256]{};
                wchar_t folderPath[MAX_PATH]{};
                GetWindowTextW(g_editRobotName, robotName, 256);
                GetWindowTextW(g_editFolderPath, folderPath, MAX_PATH);

                if (wcslen(robotName) == 0)
                {
                    MessageBoxW(hwnd, L"ロボット名を入力してください。", L"エラー", MB_OK | MB_ICONWARNING);
                    SetFocus(g_editRobotName);
                    return 0;
                }

                // Javaファイル生成・コンパイル・Robocode起動をまとめた関数を呼び出す
                if (ExportJavaCustom(blocks, folderPath, robotName, g_userFunctions))
                {
                    // 成功時の処理（必要であれば）
                }
            }
            return 0;

            // Java出力
        case ID_TOOL_JAVA:
            if (notification == BN_CLICKED)
            {
                MessageBoxW(
                    hwnd,
                    L"Java出力処理は、既存のJava生成処理を確認してから接続します。",
                    L"Java出力",
                    MB_OK | MB_ICONINFORMATION
                );
            }
            return 0;
            // ====================================================
            // キーボードショートカット
            // ====================================================

        case ID_SHORTCUT_SELECT_ALL:
            ActiveManager().SelectAllBlocks();
            InvalidateEditor();
            return 0;

        case ID_SHORTCUT_SAVE:
            SaveProject(hwnd);
            return 0;

        case ID_SHORTCUT_LOAD:
            LoadProject(hwnd);
            InvalidateEditor();
            return 0;

        case ID_SHORTCUT_UNDO:
            if (ActiveManager().Undo())
                InvalidateEditor();
            return 0;

        case ID_SHORTCUT_REDO:
            if (ActiveManager().Redo())
                InvalidateEditor();
            return 0;

        case ID_SHORTCUT_COPY:
            ActiveManager().CopySelectedBlock();
            return 0;

        case ID_SHORTCUT_CUT:
        {
            if (ActiveManager().CopySelectedBlock() &&
                ActiveManager().DeleteSelectedBlock())
            {
                InvalidateEditor();
            }
            return 0;
        }
        case ID_SHORTCUT_PASTE:
        {
            const auto& blocks = ActiveManager().GetBlocks();
            int index = ActiveManager().GetSelectedBlockIndex();

            int x = 0;
            int y = 0;

            if (index >= 0 &&
                index < static_cast<int>(blocks.size()))
            {
                x = blocks[index].x + 30;
                y = blocks[index].y + 30;
            }
            else
            {
                POINT cursor;

                if (!GetCursorPos(&cursor))
                    return 0;

                ScreenToClient(hwnd, &cursor);

                if (!IsCanvasPoint(cursor))
                    return 0;

                POINT world = ActiveManager().ScreenToWorld(
                    cursor.x,
                    cursor.y
                );

                x = world.x;
                y = world.y;
            }

            if (ActiveManager().PasteCopiedBlock(x, y))
                InvalidateEditor();

            return 0;
        }


        case ID_SHORTCUT_DELETE:
            if (ActiveManager().DeleteSelectedBlock())
                InvalidateEditor();
            return 0;
        case ID_ADD:
            if (notification == BN_CLICKED)
            {
                POINT center = {
                    g_canvasLeft + 100,
                    CANVAS_TOP + 100
                };

                POINT world =
                    ActiveManager().ScreenToWorld(
                        center.x,
                        center.y
                    );

                LRESULT selectedIndex = SendMessageW(
                    g_blockList,
                    LB_GETCURSEL,
                    0,
                    0
                );

                int index = selectedIndex == LB_ERR
                    ? 0
                    : static_cast<int>(selectedIndex);

                if (!g_visibleBlockTypes.empty())
                {
                    if (index >= 0 &&
                        index < static_cast<int>(
                            g_visibleBlockTypes.size()))
                    {
                        ActiveManager().AddBlock(
                            GetBlockTypeFromIndex(index),
                            world.x,
                            world.y
                        );

                        InvalidateEditor();
                    }
                }
            }
            return 0;

        case ID_DELETE:
            if (notification == BN_CLICKED)
            {
                g_deleteMode = !g_deleteMode;

                ActiveManager().SetDeleteMode(g_deleteMode);

                UpdateButtonColors();
                InvalidateEditor();
            }
            return 0;

        case ID_RESET_VIEW:
            if (notification == BN_CLICKED)
            {
                g_zoom = INITIAL_ZOOM;
                g_viewX = INITIAL_VIEW_X;
                g_viewY = INITIAL_VIEW_Y;

                UpdateView();
                InvalidateEditor();
            }
            return 0;

        case ID_SAVE:
            if (notification == BN_CLICKED)
                SaveProject(hwnd);
            return 0;

        case ID_LOAD:
            if (notification == BN_CLICKED)
            {
                LoadProject(hwnd);
                InvalidateEditor();
            }
            return 0;

            //操作方法ボタンが押されたときのポップアップ処理
        case ID_HELP_BUTTON:
            if (notification == BN_CLICKED)
            {
                MessageBoxW(
                    hwnd,
                    L"【 ロボコード ブロックエディタ 操作ガイド 】\n\n"
                    L"■ ブロックの基本操作\n"
                    L"  ・追加      : リストから項目を選択 ＞ 「追加」ボタン\n"
                    L"  ・移動      : キャンバス上でドラッグ＆ドロップ\n"
                    L"  ・削除      : 「削除モード」をONにしてブロックをクリック\n\n"
                    L"■ カメラ・画面操作\n"
                    L"  ・スクロール: 右クリック ＋ ドラッグ\n"
                    L"  ・拡大/縮小 : マウスホイール回転\n"
                    L"  ・表示リセット: 「表示リセット」ボタン\n\n"
                    L"■ キーボードショートカット\n"
                    L"  ・保存 / 読み込み : Ctrl + S / Ctrl + O\n"
                    L"  ・元に戻す / やり直し: Ctrl + Z / Ctrl + Y\n"
                    L"  ・コピー / 貼り付け : Ctrl + C / Ctrl + V\n"
                    L"  ・全選択 / 削除     : Ctrl + A / Delete",
                    L"操作方法とヘルプ",
                    MB_OK | MB_ICONINFORMATION
                );

            }
            return 0;

        case ID_UNDO:
            if (notification == BN_CLICKED)
            {
                if (ActiveManager().Undo())
                    InvalidateEditor();
            }
            return 0;

        case ID_REDO:
            if (notification == BN_CLICKED)
            {
                if (ActiveManager().Redo())
                    InvalidateEditor();
            }
            return 0;
        }

        break;
    }

    // ========================================================
    // マウスホイール：ズーム
    // ========================================================

    case WM_MOUSEWHEEL:
    {
        POINT mouse = {
            GET_X_LPARAM(lParam),
            GET_Y_LPARAM(lParam)
        };

        ScreenToClient(hwnd, &mouse);

        float oldZoom = g_zoom;

        float factor =
            GET_WHEEL_DELTA_WPARAM(wParam) > 0
            ? 1.1f
            : 1.0f / 1.1f;

        float newZoom = oldZoom * factor;

        newZoom = (std::max)(
            MIN_ZOOM,
            (std::min)(newZoom, MAX_ZOOM)
            );

        float worldX = (mouse.x - g_viewX) / oldZoom;
        float worldY = (mouse.y - g_viewY) / oldZoom;

        g_zoom = newZoom;
        g_viewX = mouse.x - worldX * g_zoom;
        g_viewY = mouse.y - worldY * g_zoom;

        UpdateView();
        InvalidateEditor();

        return 0;
    }

    // ========================================================
    // 右クリック：キャンバス移動
    // ========================================================

    case WM_RBUTTONDOWN:
    {
        POINT p = {
            GET_X_LPARAM(lParam),
            GET_Y_LPARAM(lParam)
        };

        if (IsCanvasPoint(p))
        {
            g_panning = true;
            g_lastPanPoint = p;

            SetCapture(hwnd);
        }

        return 0;
    }

    case WM_RBUTTONUP:
    {
        if (g_panning)
        {
            g_panning = false;

            if (GetCapture() == hwnd)
                ReleaseCapture();
        }

        return 0;
    }

    // ========================================================
    // マウス移動
    // ========================================================

    case WM_MOUSEMOVE:
    {
        POINT p = {
            GET_X_LPARAM(lParam),
            GET_Y_LPARAM(lParam)
        };

        // リストが開いていて、かつキャンバス境界線付近にマウスがある場合のみカーソルを変更
        if (g_showBlockList && g_blockList && IsWindowVisible(g_blockList))
        {
            if (abs(p.x - g_canvasLeft) <= 6 && p.y >= 50)
            {
                SetCursor(LoadCursor(nullptr, IDC_SIZEWE));
            }
        }

        // キャンバス移動
        if (g_panning)
        {
            g_viewX += p.x - g_lastPanPoint.x;
            g_viewY += p.y - g_lastPanPoint.y;

            g_lastPanPoint = p;

            UpdateView();
            InvalidateCanvas();

            return 0;
        }

        if (g_isResizingList)
        {
            int oldCanvasLeft = g_canvasLeft;
            int newCanvasLeft = p.x;
            if (newCanvasLeft >= 310 && newCanvasLeft <= 500) // 最小・最大の幅制限
            {
                g_canvasLeft = newCanvasLeft;

                // リストの左端をカテゴリーボタンの右側（220px）に固定し、幅を算出
                int listLeft = 220;
                int listWidth = g_canvasLeft - listLeft - 10;
                if (listWidth < 80) listWidth = 80;
                g_listWidth = listWidth;

                HDWP hdwp = BeginDeferWindowPos(2);

                if (hdwp)
                {
                    if (g_blockList)
                    {
                        hdwp = DeferWindowPos(
                            hdwp,
                            g_blockList,
                            nullptr,
                            listLeft, 60, g_listWidth, 475,
                            SWP_NOZORDER | SWP_NOACTIVATE
                        );
                    }

                    if (hdwp && g_createdFuncList)
                    {
                        hdwp = DeferWindowPos(
                            hdwp,
                            g_createdFuncList,
                            nullptr,
                            listLeft, 95, g_listWidth, 500,
                            SWP_NOZORDER | SWP_NOACTIVATE
                        );
                    }

                    if (hdwp)
                    {
                        EndDeferWindowPos(hdwp);
                    }
                }
                else
                {
                    // まとめて実行できなかった場合は通常の方法で更新
                    if (g_blockList)
                    {
                        SetWindowPos(
                            g_blockList, nullptr,
                            listLeft, 60, g_listWidth, 475,
                            SWP_NOZORDER | SWP_NOACTIVATE
                        );
                    }

                    if (g_createdFuncList)
                    {
                        SetWindowPos(
                            g_createdFuncList, nullptr,
                            listLeft, 95, g_listWidth, 500,
                            SWP_NOZORDER | SWP_NOACTIVATE
                        );
                    }
                }

                UpdateView();
                // 変更されたキャンバス領域を再描画
                RECT dirtyRect{};
                GetClientRect(hwnd, &dirtyRect);

                // 変化した境界より右側だけを再描画する
                dirtyRect.left = (std::min)(oldCanvasLeft, g_canvasLeft);

                // 上部ツールバー（Y座標 0～49）は再描画対象から外す
                dirtyRect.top = 50;

                InvalidateRect(hwnd, &dirtyRect, FALSE);
            }
            return 0;
        }

        // リストの右端にカーソルがあるときだけ、左右の矢印にする
        bool onListRightEdge = false;

        if (g_showBlockList &&
            g_blockList &&
            IsWindowVisible(g_blockList))
        {
            RECT listRc{};
            GetWindowRect(g_blockList, &listRc);

            ScreenToClient(hwnd, reinterpret_cast<LPPOINT>(&listRc.left));
            ScreenToClient(hwnd, reinterpret_cast<LPPOINT>(&listRc.right));

            if (abs(p.x - listRc.right) <= 10 &&
                p.y >= listRc.top &&
                p.y <= listRc.bottom)
            {
                onListRightEdge = true;
            }
        }

        if (onListRightEdge && !g_draggingFromList)
        {
            SetCursor(LoadCursor(nullptr, IDC_SIZEWE));
        }
        else if (!g_isResizingList && !g_draggingFromList)
        {
            SetCursor(LoadCursor(nullptr, IDC_ARROW));
        }


        // リストからブロックをドラッグ中
        if (g_draggingFromList)
        {
            // 現在の位置を更新（メインウィンドウのクライアント座標）
            g_listDragPoint = p;

            // メインウィンドウの座標をスクリーン座標に変換
            POINT screenPoint = g_listDragPoint;
            ClientToScreen(hwnd, &screenPoint);

            // プレビュー専用ウィンドウをカーソル位置へ移動
            if (g_dragPreviewWnd)
            {
                SetWindowPos(
                    g_dragPreviewWnd,
                    HWND_TOPMOST,
                    screenPoint.x - 84,
                    screenPoint.y - 15,
                    168,
                    30,
                    SWP_NOACTIVATE | SWP_SHOWWINDOW
                );

                InvalidateRect(g_dragPreviewWnd, nullptr, FALSE);
            }

            return 0;
        }




        ActiveManager().OnMouseMove(p.x, p.y);

        // 左ボタンを押している間
        if (wParam & MK_LBUTTON)
        {
            if (ActiveManager().IsSelectingRange())
            {
                // 範囲選択はワールド座標で更新
                POINT world = ActiveManager().ScreenToWorld(
                    p.x,
                    p.y
                );

                ActiveManager().UpdateRangeSelection(
                    world.x,
                    world.y
                );
            }
            else
            {
                // 通常のブロック移動
                ActiveManager().OnMouseMove(p.x, p.y);
            }
        }
        else
        {
            // クリックしていなくてもカーソル位置を更新
            ActiveManager().OnMouseMove(p.x, p.y);
        }

        // カーソルが動くたびに再描画
        InvalidateCanvas();

        return 0;
    }

    // ========================================================
    // 左クリック：ブロック操作・範囲選択
    // ========================================================


    case WM_LBUTTONDOWN:
    {
        POINT p = {
            GET_X_LPARAM(lParam),
            GET_Y_LPARAM(lParam)
        };


        //リストが開いているとき、その右端（境界線）をクリックしたらリサイズ開始
        if (g_showBlockList && g_blockList && IsWindowVisible(g_blockList))
        {
            // キャンバス左端の境界線
            if (abs(p.x - g_canvasLeft) <= 6 && p.y >= 50)
            {
                g_isResizingList = true;
                SetCapture(hwnd);
                return 0;
            }

            // リストの右端（境界線）
            RECT listRc;
            GetWindowRect(g_blockList, &listRc);
            ScreenToClient(hwnd, (LPPOINT)&listRc.left);
            ScreenToClient(hwnd, (LPPOINT)&listRc.right);

            if (abs(p.x - listRc.right) <= 6 && p.y >= listRc.top && p.y <= listRc.bottom)
            {
                g_isResizingList = true;
                SetCapture(hwnd);
                return 0;
            }
        }



        // Java出力ボタン
        if (p.x >= 10 && p.x <= 140 &&
            p.y >= 10 && p.y <= 48)
        {
            if (g_isEditingFunction)
            {
                MessageBoxW(hwnd, L"ロボットとして出力するには、先に「メイン」タブへ戻ってください。",
                    L"Java出力", MB_OK | MB_ICONINFORMATION);
                return 0;
            }
            const auto& blocks = g_manager.GetBlocks();
            if (blocks.empty())
            {
                MessageBoxW(hwnd, L"ブロックが配置されていません。", L"警告", MB_OK | MB_ICONWARNING);
                return 0;
            }

            // 画面のテキストボックスから「ロボット名」と「保存先フォルダ」を取得
            wchar_t robotName[256]{};
            wchar_t folderPath[MAX_PATH]{};
            GetWindowTextW(g_editRobotName, robotName, 256);
            GetWindowTextW(g_editFolderPath, folderPath, MAX_PATH);

            if (wcslen(robotName) == 0)
            {
                MessageBoxW(hwnd, L"ロボット名を入力してください。", L"エラー", MB_OK | MB_ICONWARNING);
                SetFocus(g_editRobotName);
                return 0;
            }

            // 3つの引数を渡して実行
            ExportJavaCustom(blocks, folderPath, robotName, g_userFunctions);
            return 0;
        }

        if (!IsCanvasPoint(p))
            return 0;

        POINT world = ActiveManager().ScreenToWorld(p.x, p.y);

        // 色変更ブロックをクリックした可能性を記録する
        // ========================================================
        // 色変更ブロックの色見本部分をクリックしたか判定
        // ========================================================

        g_colorClickCandidate = false;

        const auto& colorBlocks = ActiveManager().GetBlocks();

        if ((GetKeyState(VK_CONTROL) & 0x8000) == 0)
        {
            for (int i = static_cast<int>(colorBlocks.size()) - 1;
                i >= 0; --i)
            {
                const Block& b = colorBlocks[static_cast<size_t>(i)];

                // ブロックの範囲内か確認
                if (world.x < b.x ||
                    world.x >= b.x + b.w ||
                    world.y < b.y ||
                    world.y >= b.y + b.h)
                {
                    continue;
                }

                // 色変更ブロックか確認
                bool isColorBlock = false;

                switch (b.type)
                {
                case BlockType::SetBodyColor:
                case BlockType::SetGunColor:
                case BlockType::SetRadarColor:
                case BlockType::SetBulletColor:
                case BlockType::SetScanColor:
                    isColorBlock = true;
                    break;

                default:
                    break;
                }

                // 最前面のブロックが色変更ブロックでなければ終了
                if (!isColorBlock)
                    break;

                // 色見本部分をクリックした場合だけ編集対象にする
                if (IsColorEditArea(b, world))
                {
                    g_colorClickCandidate = true;
                    g_colorClickStart = p;
                }

                break;
            }
        }

        // 数値入力ブロックなら数値入力欄を開く
        if (TryStartNumericEdit(hwnd, p, world))
            return 0;
        if (g_numericEdit)
            FinishNumericEdit(hwnd, true);

        const auto& blocks = ActiveManager().GetBlocks();
        bool hitBlock = false;

        // 手前のブロックから判定する
        for (int i = static_cast<int>(blocks.size()) - 1;
            i >= 0; --i)
        {
            const Block& block = blocks[i];

            // 描画時と同じ HitBlock 判定を使用する
            if (HitBlock(block, world.x, world.y))
            {
                hitBlock = true;
                break;
            }
        }

        // 削除モードでは、クリックしたブロックを削除
        if (g_deleteMode)
        {
            if (hitBlock)
            {
                ActiveManager().DeleteBlockAt(world.x, world.y);
                InvalidateEditor();
            }

            return 0;
        }

        // 空白からドラッグしたら範囲選択
        if (!hitBlock)
        {
            ActiveManager().StartRangeSelection(world.x, world.y);

            SetCapture(hwnd);
            InvalidateEditor();

            return 0;
        }

        // 色変更ブロックをクリックした場合は色を編集する。
        // ただし、Ctrlキーを押しながらなら通常の選択・移動を優先する。
        if ((GetKeyState(VK_CONTROL) & 0x8000) == 0)
        {
            if (TryStartColorEdit(hwnd, world))
                return 0;
        }

        // ブロックをクリックしたら通常の選択・移動
        ActiveManager().OnMouseDown(hwnd, p.x, p.y);

        SetCapture(hwnd);
        InvalidateEditor();

        return 0;
    }


    // ========================================================
    // 左ボタンを離す：ドラッグ・範囲選択終了
    // ========================================================

    case WM_LBUTTONUP:
    {
        if (g_isResizingList)
        {
            g_isResizingList = false;

            if (g_blockList && IsWindowVisible(g_blockList))
            {
                RECT btnRc{};
                GetWindowRect(g_tabFuncBtn, &btnRc);
                POINT ptTopLeft = { btnRc.right + 5, btnRc.top };
                ScreenToClient(hwnd, &ptTopLeft);

                int listScreenRight = ptTopLeft.x + g_listWidth;
                POINT worldListRight = ActiveManager().ScreenToWorld(listScreenRight, CANVAS_TOP);

                int targetMinX = worldListRight.x + 45;

                auto blocks = ActiveManager().GetBlocks();

                // まだバックアップがなければ（念のため）、現在の位置を初期位置として保存しておく
                if (g_originalXMap.empty())
                {
                    for (size_t i = 0; i < blocks.size(); ++i)
                    {
                        g_originalXMap[i] = blocks[i].x - g_blockShiftMap[i]; // 過去の移動分を引いた位置を記録
                    }
                }

                int maxShiftNeeded = 0;
                for (const auto& b : blocks)
                {
                    if (b.x < targetMinX)
                    {
                        int needed = targetMinX - b.x;
                        if (needed > maxShiftNeeded)
                        {
                            maxShiftNeeded = needed;
                        }
                    }
                }

                if (maxShiftNeeded > 0)
                {
                    for (size_t i = 0; i < blocks.size(); ++i)
                    {
                        Block b = blocks[i];
                        if (b.x < targetMinX + 250)
                        {
                            b.x += maxShiftNeeded;
                            blocks[i] = b;
                            ActiveManager().UpdateBlockAt(static_cast<int>(i), b);
                        }
                    }
                }
                UpdateView();
            }

            if (GetCapture() == hwnd) ReleaseCapture();
            InvalidateEditor();
            return 0;
        }

        // 色変更ブロックのクリック操作を判定する
        if (g_colorClickCandidate)
        {
            g_colorClickCandidate = false;

            POINT p = {
                GET_X_LPARAM(lParam),
                GET_Y_LPARAM(lParam)
            };

            int dx = abs(p.x - g_colorClickStart.x);
            int dy = abs(p.y - g_colorClickStart.y);

            // ほとんど移動していない場合だけクリックとみなす
            if (dx <= 4 && dy <= 4)
            {
                POINT world = ActiveManager().ScreenToWorld(
                    p.x, p.y
                );

                if (TryStartColorEdit(hwnd, world))
                {
                    // マウス捕捉を解除して編集終了
                    if (GetCapture() == hwnd)
                        ReleaseCapture();

                    return 0;
                }
            }
        }
        // リストからドラッグしたブロックをドロップ
        if (g_draggingFromList)
        {
            POINT p = {
                GET_X_LPARAM(lParam),
                GET_Y_LPARAM(lParam)
            };

            // ドラッグ状態を先に解除
            g_draggingFromList = false;

            // ドラッグプレビューを非表示にする
            if (g_dragPreviewWnd)
                ShowWindow(g_dragPreviewWnd, SW_HIDE);

            if (IsCanvasPoint(p))
            {
                //// キャンバス座標をブロックのワールド座標に変換
                //POINT world = ActiveManager().ScreenToWorld(
                //    p.x,
                //    p.y
                //);

                // ブロックの幅168、高さ30の半分を引く
                POINT worldTopLeft = ActiveManager().ScreenToWorld(
                    p.x - 84,
                    p.y - 15
                );

                // ドロップ位置にブロックを追加。
                // 自作関数の場合は、関数名も一緒に保存する。
                Block newBlock{};
                newBlock.type = g_dragBlockType;
                newBlock.x = worldTopLeft.x;
                newBlock.y = worldTopLeft.y;
                if (!g_dragBlockFunctionName.empty())
                    SetUserFunctionCallName(newBlock, g_dragBlockFunctionName);
                ActiveManager().AddBlock(newBlock);
            }

            g_dragBlockFunctionName.clear();
            if (GetCapture() == hwnd)
                ReleaseCapture();

            InvalidateEditor();

            return 0;
        }

        if (ActiveManager().IsSelectingRange())
        {
            POINT p = {
                GET_X_LPARAM(lParam),
                GET_Y_LPARAM(lParam)
            };

            POINT world = ActiveManager().ScreenToWorld(
                p.x,
                p.y
            );

            // 最終位置を反映
            ActiveManager().UpdateRangeSelection(
                world.x,
                world.y
            );

            ActiveManager().EndRangeSelection();
        }
        else
        {
            POINT p = {
                GET_X_LPARAM(lParam),
                GET_Y_LPARAM(lParam)
            };

            // マウスを離す前にドラッグ対象の番号を取得
            int draggedIndex = ActiveManager().GetDraggedBlockIndex();

            // ドラッグ操作を終了
            ActiveManager().OnMouseUp(hwnd);

            // 黒い部分で離した場合、ブロックと子ブロックを削除
            if (IsBlackArea(p) && draggedIndex >= 0)
            {
                ActiveManager().DeleteBlockAndChildren(draggedIndex);
            }
        }

        if (GetCapture() == hwnd)
            ReleaseCapture();

        InvalidateEditor();

        return 0;
    }

    // ========================================================
    // マウス捕捉の変更
    // ========================================================

    case WM_CAPTURECHANGED:
    {
        g_panning = false;
        return 0;
    }

    // ========================================================
    // 画面描画
    // ========================================================

    case WM_PAINT:
    {
        PAINTSTRUCT ps{};
        HDC hdc = BeginPaint(hwnd, &ps);

        RECT rc{};
        GetClientRect(hwnd, &rc);

        HDC memDC = CreateCompatibleDC(hdc);

        HBITMAP memBitmap = CreateCompatibleBitmap(
            hdc,
            rc.right,
            rc.bottom
        );

        HGDIOBJ oldBitmap = SelectObject(
            memDC,
            memBitmap
        );

        // ① 新しい画面レイアウトを先に描画
        DrawNewLayout(
            memDC,
            rc.right,
            rc.bottom
        );

        // ② キャンバスの範囲
        RECT canvasRect = {
            g_canvasLeft,
            CANVAS_TOP,
            rc.right - 20,
            rc.bottom - 20
        };


        DrawCanvasGrid(memDC, canvasRect);

        // ③ 既存のブロックと選択枠を描画
        ActiveManager().DrawAll(memDC);

        // 関数編集画面であることと、基本操作をキャンバス上にも表示。
        if (g_isEditingFunction)
        {
            RECT functionTitleRect{ g_canvasLeft + 18, 54, (std::min)(static_cast<int>(rc.right) - 24, g_canvasLeft + 620), 78 };
            DrawLabel(memDC, (L"編集中の関数： " + g_currentFunctionName).c_str(),
                functionTitleRect, RGB(45, 55, 72), 16, true);
            RECT functionGuideRect{ g_canvasLeft + 18, 78, (std::min)(static_cast<int>(rc.right) - 24, g_canvasLeft + 930), 100 };
            DrawLabel(memDC, L"左のカテゴリからドラッグして追加 / 数字をクリックして変更 / Deleteで削除 / Ctrl+Zで元に戻す / 上の「関数」で一覧へ",
                functionGuideRect, RGB(72, 82, 98), 12, false);
        }



        // ⑤ ズーム表示
        wchar_t zoomText[64]{};

        swprintf_s(
            zoomText,
            L"Zoom: %.0f%%",
            g_zoom * 100.0f
        );

        SetBkMode(memDC, TRANSPARENT);
        SetTextColor(memDC, RGB(45, 55, 72));

        TextOutW(
            memDC,
            g_canvasLeft,
            15,
            zoomText,
            static_cast<int>(wcslen(zoomText))
        );

        // ⑥ 描画結果を画面へ転送
        const RECT& dirty = ps.rcPaint;

        const int dirtyWidth = dirty.right - dirty.left;
        const int dirtyHeight = dirty.bottom - dirty.top;

        if (dirtyWidth > 0 && dirtyHeight > 0)
        {
            BitBlt(
                hdc,
                dirty.left,
                dirty.top,
                dirtyWidth,
                dirtyHeight,
                memDC,
                dirty.left,
                dirty.top,
                SRCCOPY
            );
        }

        // ⑦ ダブルバッファを解放
        SelectObject(memDC, oldBitmap);
        DeleteObject(memBitmap);
        DeleteDC(memDC);

        EndPaint(hwnd, &ps);

        return 0;
    }

    // ========================================================
    // 終了処理
    // ========================================================

    case WM_DESTROY:
    {
        if (g_numericEdit)
            FinishNumericEdit(hwnd, true);

        // 終了時に関数編集状態を解除
        g_isEditingFunction = false;
        g_currentFunctionName.clear();

        // 終了時に自作関数の保存ファイルを削除する
        if (!ClearUserFunctionStorage())
        {
            MessageBoxW(
                hwnd,
                L"自作関数の保存ファイルを削除できませんでした。",
                L"終了処理の警告",
                MB_OK | MB_ICONWARNING
            );
        }

        // 現在のプロジェクトで使用している関数一覧をリセット
        g_userFunctions.clear();

        // 関数編集用のブロックをリセット
        g_functionManager.ReplaceBlocks({}, {});

        if (g_blockList && g_oldListProc)
        {
            SetWindowLongPtrW(
                g_blockList,
                GWLP_WNDPROC,
                reinterpret_cast<LONG_PTR>(
                    g_oldListProc
                    )
            );
        }

        // GDI+ Bitmap オブジェクトの解放
        if (g_hBmpNew) { DeleteObject(g_hBmpNew);   g_hBmpNew = nullptr; }
        if (g_hBmpOpen) { DeleteObject(g_hBmpOpen);  g_hBmpOpen = nullptr; }
        if (g_hBmpSave) { DeleteObject(g_hBmpSave);  g_hBmpSave = nullptr; }
        if (g_hBmpUndo) { DeleteObject(g_hBmpUndo);  g_hBmpUndo = nullptr; }
        if (g_hBmpRedo) { DeleteObject(g_hBmpRedo);  g_hBmpRedo = nullptr; }
        if (g_hBmpRun) { DeleteObject(g_hBmpRun);   g_hBmpRun = nullptr; }

        PostQuitMessage(0);
        return 0;
    }
    }

    return DefWindowProcW(
        hwnd,
        msg,
        wParam,
        lParam
    );


}