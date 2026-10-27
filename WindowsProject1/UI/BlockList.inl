void InitializeBlockList(HWND list)
{
    if (!list)
        return;

    SendMessageW(
        list,
        LB_RESETCONTENT,
        0,
        0
    );

    g_visibleBlockTypes = GetBlockTypes(g_selectedCategory);
    g_visibleFunctionNames.assign(g_visibleBlockTypes.size(), L"");

    for (BlockType type : g_visibleBlockTypes)
    {
        std::wstring name = GetBlockName(type);
        SendMessageW(list, LB_ADDSTRING, 0,
            reinterpret_cast<LPARAM>(name.c_str()));
    }

    // 「関数」カテゴリの末尾に、ユーザーが作った関数を呼び出す項目を追加。
    if (g_selectedCategory == BlockCategory::Function)
    {
        for (const auto& function : g_userFunctions)
        {
            // 編集中の関数自身は一覧に出さず、間違った自己呼び出しを防ぐ。
            if (g_isEditingFunction && function.name == g_currentFunctionName)
                continue;

            g_visibleBlockTypes.push_back(BlockType::DoNothing);
            g_visibleFunctionNames.push_back(function.name);
            const std::wstring callLabel = L"自作関数： " + function.name;
            SendMessageW(list, LB_ADDSTRING, 0,
                reinterpret_cast<LPARAM>(callLabel.c_str()));
        }
    }

    SendMessageW(
        list,
        LB_SETCURSEL,
        static_cast<WPARAM>(-1),
        0
    );
}

// ============================================================
// リストの選択番号からブロック種類を取得
// ============================================================

BlockType GetBlockTypeFromIndex(int index)
{
    if (index < 0 ||
        index >= static_cast<int>(
            g_visibleBlockTypes.size()))
    {
        return BlockType::Move;
    }

    return g_visibleBlockTypes[index];
}

// ============================================================
// 表示更新
// ============================================================

void InvalidateEditor()
{
    if (g_mainWindow)
    {
        InvalidateRect(
            g_mainWindow,
            nullptr,
            FALSE
        );
    }
}

void InvalidateCanvas()
{
    if (!g_mainWindow)
        return;

    RECT rc{};
    GetClientRect(g_mainWindow, &rc);

    RECT canvasRect = {
        g_canvasLeft,
        CANVAS_TOP,
        rc.right,
        rc.bottom
    };

    InvalidateRect(
        g_mainWindow,
        &canvasRect,
        FALSE
    );
}

// ============================================================
// 既存ボタンの色を更新
// ============================================================

void UpdateButtonColors()
{
    if (g_addButton)
    {
        RedrawWindow(
            g_addButton,
            nullptr,
            nullptr,
            RDW_INVALIDATE | RDW_UPDATENOW
        );
    }

    if (g_deleteButton)
    {
        RedrawWindow(
            g_deleteButton,
            nullptr,
            nullptr,
            RDW_INVALIDATE | RDW_UPDATENOW
        );
    }
}

// ============================================================
// ボタンを描画する共通処理
// ============================================================

void DrawCustomButton(
    const DRAWITEMSTRUCT* dis,
    COLORREF color,
    const wchar_t* text)
{
    HDC hdc = dis->hDC;
    RECT rc = dis->rcItem;

    HBRUSH brush = CreateSolidBrush(color);
    if (brush)
    {
        FillRect(hdc, &rc, brush);
        DeleteObject(brush);
    }

    HBRUSH borderBrush =
        CreateSolidBrush(RGB(220, 220, 220));

    FrameRect(hdc, &rc, borderBrush);
    DeleteObject(borderBrush);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(255, 255, 255));

    HFONT font = reinterpret_cast<HFONT>(
        SendMessageW(
            dis->hwndItem,
            WM_GETFONT,
            0,
            0
        )
        );

    HGDIOBJ oldFont = nullptr;

    if (font)
        oldFont = SelectObject(hdc, font);

    DrawTextW(
        hdc,
        text,
        -1,
        &rc,
        DT_CENTER | DT_VCENTER | DT_SINGLELINE
    );

    if (oldFont)
        SelectObject(hdc, oldFont);
}

// ============================================================
// カテゴリを切り替える
// ============================================================

