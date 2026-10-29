int FindUserFunctionIndex(const std::wstring& name)
{

    for (int i = 0; i < static_cast<int>(g_userFunctions.size()); ++i)
    {
        if (g_userFunctions[static_cast<size_t>(i)].name == name)
            return i;
    }
    return -1;
}

void ReloadUserFunctions()
{
    std::vector<UserDefinedFunction> loaded;
    if (LoadUserFunctions(loaded))
        g_userFunctions = std::move(loaded);
}

std::wstring TrimFunctionName(std::wstring value)
{
    auto isSpace = [](wchar_t ch) { return ch == L' ' || ch == L'\t' || ch == L'\r' || ch == L'\n'; };
    while (!value.empty() && isSpace(value.front())) value.erase(value.begin());
    while (!value.empty() && isSpace(value.back())) value.pop_back();
    return value;
}

struct FunctionNameDialogState
{
    HWND edit = nullptr;
    std::wstring value;
    bool accepted = false;
};

constexpr int ID_FUNCTION_NAME_EDIT = 1;
constexpr int ID_FUNCTION_NAME_OK = 2;
constexpr int ID_FUNCTION_NAME_CANCEL = 3;

LRESULT CALLBACK FunctionNameDialogProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    auto* state = reinterpret_cast<FunctionNameDialogState*>(
        GetWindowLongPtrW(window, GWLP_USERDATA));

    if (message == WM_NCCREATE)
    {
        const auto* createInfo = reinterpret_cast<const CREATESTRUCTW*>(lParam);
        state = static_cast<FunctionNameDialogState*>(createInfo->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
    }

    switch (message)
    {
    case WM_CREATE:
        CreateWindowW(L"STATIC", L"関数の名前を入力してください。", WS_CHILD | WS_VISIBLE,
            18, 16, 340, 24, window, nullptr, GetModuleHandleW(nullptr), nullptr);
        state->edit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"MyFunction",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | WS_TABSTOP,
            18, 44, 340, 28, window,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_FUNCTION_NAME_EDIT)),
            GetModuleHandleW(nullptr), nullptr);
        CreateWindowW(L"BUTTON", L"作成", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
            184, 88, 80, 30, window,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_FUNCTION_NAME_OK)),
            GetModuleHandleW(nullptr), nullptr);
        CreateWindowW(L"BUTTON", L"キャンセル", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            274, 88, 84, 30, window,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_FUNCTION_NAME_CANCEL)),
            GetModuleHandleW(nullptr), nullptr);
        SetFocus(state->edit);
        SendMessageW(state->edit, EM_SETSEL, 0, -1);
        return 0;

    case WM_COMMAND:
        if (LOWORD(wParam) == ID_FUNCTION_NAME_OK)
        {
            wchar_t buffer[256]{};
            GetWindowTextW(state->edit, buffer, 256);
            state->value = TrimFunctionName(buffer);
            state->accepted = true;
            DestroyWindow(window);
            return 0;
        }
        if (LOWORD(wParam) == ID_FUNCTION_NAME_CANCEL)
        {
            DestroyWindow(window);
            return 0;
        }
        break;

    case WM_CLOSE:
        DestroyWindow(window);
        return 0;
    }

    return DefWindowProcW(window, message, wParam, lParam);
}

bool PromptForFunctionName(HWND owner, std::wstring& result)
{
    static bool classRegistered = false;
    const wchar_t* className = L"BlockEditorFunctionNameDialog";

    if (!classRegistered)
    {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = FunctionNameDialogProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
        wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        wc.lpszClassName = className;
        if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
            return false;
        classRegistered = true;
    }

    FunctionNameDialogState state;
    HWND dialog = CreateWindowExW(
        WS_EX_DLGMODALFRAME,
        className,
        L"新しい関数を作成",
        WS_CAPTION | WS_SYSMENU | WS_POPUP,
        CW_USEDEFAULT, CW_USEDEFAULT, 390, 165,
        owner, nullptr, GetModuleHandleW(nullptr), &state);

    if (!dialog)
        return false;

    if (owner) EnableWindow(owner, FALSE);
    RECT ownerRect{};
    if (owner && GetWindowRect(owner, &ownerRect))
    {
        SetWindowPos(dialog, HWND_TOP,
            ownerRect.left + ((ownerRect.right - ownerRect.left) - 390) / 2,
            ownerRect.top + ((ownerRect.bottom - ownerRect.top) - 165) / 2,
            390, 165, SWP_SHOWWINDOW);
    }
    else
    {
        ShowWindow(dialog, SW_SHOW);
    }
    UpdateWindow(dialog);

    MSG msg{};
    while (IsWindow(dialog))
    {
        const BOOL gotMessage = GetMessageW(&msg, nullptr, 0, 0);
        if (gotMessage <= 0)
        {
            if (IsWindow(dialog)) DestroyWindow(dialog);
            break;
        }
        if (!IsDialogMessageW(dialog, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    if (owner)
    {
        EnableWindow(owner, TRUE);
        SetForegroundWindow(owner);
    }

    if (!state.accepted)
        return false;

    result = TrimFunctionName(state.value);
    return !result.empty();
}

bool SaveCurrentFunction()
{
    if (!g_isEditingFunction || g_currentFunctionName.empty())
        return true;

    UserDefinedFunction updated;
    updated.name = g_currentFunctionName;
    updated.blocks = ActiveManager().GetBlocks();
    updated.parents = ActiveManager().GetParents();
    updated.repeatBodies = ActiveManager().GetRepeatBodies();

    if (!SaveUserFunction(updated))
    {
        MessageBoxW(g_mainWindow,
            L"関数を保存できませんでした。保存先フォルダやファイルのアクセス権を確認してください。",
            L"関数の保存エラー", MB_OK | MB_ICONERROR);
        return false;
    }

    const int index = FindUserFunctionIndex(updated.name);
    if (index >= 0)
        g_userFunctions[static_cast<size_t>(index)] = std::move(updated);
    else
        g_userFunctions.push_back(std::move(updated));

    return true;
}

void RefreshFunctionList()
{
    if (!g_createdFuncList)
        return;

    SendMessageW(g_createdFuncList, LB_RESETCONTENT, 0, 0);
    const std::wstring createLabel = L"＋ 新しい関数を作る";
    SendMessageW(g_createdFuncList, LB_ADDSTRING, 0,
        reinterpret_cast<LPARAM>(createLabel.c_str()));

    for (const auto& function : g_userFunctions)
    {
        SendMessageW(g_createdFuncList, LB_ADDSTRING, 0,
            reinterpret_cast<LPARAM>(function.name.c_str()));
    }

    SendMessageW(g_createdFuncList, LB_SETCURSEL, static_cast<WPARAM>(-1), 0);
}

void SetFunctionHelp(const wchar_t* text, bool show)
{
    if (!g_functionHelpLabel)
        return;
    SetWindowTextW(g_functionHelpLabel, text);
    ShowWindow(g_functionHelpLabel, show ? SW_SHOW : SW_HIDE);
}

int g_totalShiftX = 0;

void CloseBlockPalette()
{
    if (g_blockList)
        ShowWindow(g_blockList, SW_HIDE);
    g_showBlockList = false;

    // 記憶していた移動量（g_totalShiftX）をそのまま引き戻す
    if (g_totalShiftX > 0)
    {
        auto blocks = ActiveManager().GetBlocks();
        for (size_t i = 0; i < blocks.size(); ++i)
        {
            Block b = blocks[i];
            b.x -= g_totalShiftX; // 押し出した分をそのまま戻す
            ActiveManager().UpdateBlockAt(static_cast<int>(i), b);
        }
        g_totalShiftX = 0; // 移動量をリセット
    }


    if (g_canvasLeft != 230)
    {
        const int delta = 230 - g_canvasLeft;
        g_canvasLeft = 230;
        g_viewX += delta;
    }
    UpdateView();
    InvalidateEditor();
}

bool OpenUserFunctionEditor(const std::wstring& name)
{
    if (g_isEditingFunction && g_currentFunctionName != name)
    {
        if (!SaveCurrentFunction())
            return false;
    }

    UserDefinedFunction function;
    if (!LoadUserFunction(name, function))
    {
        MessageBoxW(g_mainWindow, L"関数を読み込めませんでした。ファイルが壊れていないか確認してください。",
            L"読み込みエラー", MB_OK | MB_ICONERROR);
        return false;
    }

    if (!g_functionManager.ReplaceBlocks(function.blocks, function.parents, function.repeatBodies))
    {
        MessageBoxW(g_mainWindow, L"関数のブロック構造が正しくないため、編集画面を開けませんでした。",
            L"読み込みエラー", MB_OK | MB_ICONERROR);
        return false;
    }

    g_currentFunctionName = name;
    g_isEditingFunction = true;
    g_activeSidebarTab = 2;
    CloseBlockPalette();
    if (g_createdFuncList) ShowWindow(g_createdFuncList, SW_HIDE);
    for (int i = 0; i < CATEGORY_COUNT; ++i)
        if (g_categoryButtons[i]) ShowWindow(g_categoryButtons[i], SW_SHOW);
    SetFunctionHelp(L"左のカテゴリからブロックを選び、一覧からキャンバスへドラッグします。\n数字の部分をクリックすると値を変更できます。\nDelete：削除 / Ctrl+Z：元に戻す", true);
    UpdateView();
    UpdateButtonColors();
    InvalidateEditor();
    return true;
}

bool CreateUserFunction(HWND owner)
{
    std::wstring name;
    if (!PromptForFunctionName(owner, name))
        return false;

    std::wstring reason;
    if (!IsValidUserFunctionName(name, &reason))
    {
        MessageBoxW(owner, reason.c_str(), L"関数名を確認してください", MB_OK | MB_ICONWARNING);
        return false;
    }

    if (FindUserFunctionIndex(name) >= 0 ||
        std::filesystem::exists(
            GetUserFunctionDirectory() / (name + L".blk")))
    {
        MessageBoxW(owner,
            L"同じ名前の関数がすでにあります。別の名前を入力してください。",
            L"名前が重複しています",
            MB_OK | MB_ICONWARNING);
        return false;
    }

    UserDefinedFunction function;
    function.name = name;
    if (!SaveUserFunction(function))
    {
        MessageBoxW(owner, L"新しい関数を保存できませんでした。", L"エラー", MB_OK | MB_ICONERROR);
        return false;
    }

    g_userFunctions.push_back(function);
    std::sort(g_userFunctions.begin(), g_userFunctions.end(),
        [](const UserDefinedFunction& a, const UserDefinedFunction& b) { return a.name < b.name; });

    return OpenUserFunctionEditor(name);
}

void ShowFunctionList()
{
    if (g_isEditingFunction && !SaveCurrentFunction())
        return;

    g_isEditingFunction = false;
    g_currentFunctionName.clear();
    g_activeSidebarTab = 1;
    CloseBlockPalette();
    for (int i = 0; i < CATEGORY_COUNT; ++i)
        if (g_categoryButtons[i]) ShowWindow(g_categoryButtons[i], SW_HIDE);

    //関数一覧を表示する際は、メインの可変リスト幅の影響をリセットし、キャンバス位置を初期位置（230）に戻す
    if (g_canvasLeft != 230)
    {
        int delta = 230 - g_canvasLeft;
        g_canvasLeft = 230;
        g_viewX += delta;
    }

    // 関数一覧リスト（g_createdFuncList）の位置と幅を常に初期の固定位置に設定する
    if (g_createdFuncList)
    {
        SetWindowPos(g_createdFuncList, nullptr, 12, 95, 202, 500, SWP_NOZORDER);
        ShowWindow(g_createdFuncList, SW_SHOW);
    }

    RefreshFunctionList();
    if (g_createdFuncList) ShowWindow(g_createdFuncList, SW_SHOW);
    SetFunctionHelp(L"「＋ 新しい関数を作る」をクリックすると作成できます。\n作成した関数をダブルクリックすると編集できます。\n関数はメイン画面の「関数」カテゴリから呼び出せます。", true);
    UpdateView();
    UpdateButtonColors();
    InvalidateEditor();
}

// ============================================================
// 座標変換を更新
// ============================================================

