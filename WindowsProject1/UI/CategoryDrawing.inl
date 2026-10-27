void DrawCategoryButton(
    const DRAWITEMSTRUCT* dis,
    int categoryIndex)
{
    if (categoryIndex < 0 ||
        categoryIndex >= CATEGORY_COUNT)
    {
        return;
    }

    bool selected =
        static_cast<int>(g_selectedCategory)
        == categoryIndex;

    COLORREF color = CATEGORY_COLORS[categoryIndex];

    bool hovered = (g_hoveredCategory == categoryIndex);

    int r = GetRValue(color);
    int g = GetGValue(color);
    int b = GetBValue(color);

    if (hovered)
    {
        // マウスが乗っているときは明るくする
        color = RGB(
            min(r + 60, 255),
            min(g + 60, 255),
            min(b + 60, 255)
        );
    }
    else if (selected)
    {
        // 選択中は少し明るくする
        color = RGB(
            min(r + 35, 255),
            min(g + 35, 255),
            min(b + 35, 255)
        );
    }
    else
    {
        // 通常時は少し暗くする
        color = RGB(r * 3 / 4, g * 3 / 4, b * 3 / 4);
    }


    // 選択中は少し明るくする
    if (selected)
    {
        color = RGB(
            GetRValue(color) > 220
            ? 255 : GetRValue(color) + 35,
            GetGValue(color) > 220
            ? 255 : GetGValue(color) + 35,
            GetBValue(color) > 220
            ? 255 : GetBValue(color) + 35
        );
    }
    else
    {
        // 非選択時は暗くする
        color = RGB(
            GetRValue(color) * 3 / 4,
            GetGValue(color) * 3 / 4,
            GetBValue(color) * 3 / 4
        );
    }

    HDC hdc = dis->hDC;
    RECT rc = dis->rcItem;

    HBRUSH brush = CreateSolidBrush(color);
    FillRect(hdc, &rc, brush);
    DeleteObject(brush);

    // 選択中は白い枠で強調
    HBRUSH borderBrush = CreateSolidBrush(
        selected
        ? RGB(255, 255, 255)
        : RGB(210, 210, 210)
    );

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
        CATEGORY_NAMES[categoryIndex],
        -1,
        &rc,
        DT_CENTER | DT_VCENTER | DT_SINGLELINE
    );

    if (oldFont)
        SelectObject(hdc, oldFont);
}

// =======================================================
// ブロック一覧の項目をカテゴリーごとに色分け
// =========================================================


void DrawBlockListItem(const DRAWITEMSTRUCT* dis)
{
    if (!dis)
        return;

    int index = static_cast<int>(dis->itemID);
    HDC hdc = dis->hDC;
    RECT rc = dis->rcItem;

    // 項目が無効な場合
    if (index < 0 ||
        index >= static_cast<int>(g_visibleBlockTypes.size()))
    {
        FillRect(hdc, &rc, GetSysColorBrush(COLOR_WINDOW));
        return;
    }

    // ブロック種類とカテゴリ色を取得
    BlockType type = g_visibleBlockTypes[index];
    COLORREF color = GetBlockCategoryColor(type);

    // 色の値をデバッグ出力
    wchar_t debug[128]{};
    swprintf_s(
        debug,
        L"index=%d, type=%d, color=0x%08X\n",
        index,
        static_cast<int>(type),
        static_cast<unsigned int>(color)
    );
    OutputDebugStringW(debug);

    // ホバー・選択状態
    bool selected = (dis->itemState & ODS_SELECTED) != 0;
    bool hovered = (index == g_hoveredBlockListItem);

    if (hovered || selected)
    {
        int amount = hovered ? 30 : 20;

        color = RGB(
            (std::min)(GetRValue(color) + amount, 255),
            (std::min)(GetGValue(color) + amount, 255),
            (std::min)(GetBValue(color) + amount, 255)
        );
    }

    // 背景色を描画
    HBRUSH brush = CreateSolidBrush(color);
    FillRect(hdc, &rc, brush);
    DeleteObject(brush);

    // ブロック名を描画
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(255, 255, 255));

    std::wstring name = GetBlockName(type);
    if (index >= 0 && index < static_cast<int>(g_visibleFunctionNames.size()) &&
        !g_visibleFunctionNames[static_cast<size_t>(index)].empty())
    {
        name = L"自作関数： " + g_visibleFunctionNames[static_cast<size_t>(index)];
    }
    rc.left += 8;

    DrawTextW(
        hdc,
        name.c_str(),
        -1,
        &rc,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE
    );

    if (dis->itemState & ODS_FOCUS)
        DrawFocusRect(hdc, &dis->rcItem);
}


void FinishNumericEdit(HWND hwnd, bool commit);

// ============================================================
// プロジェクト保存
// ============================================================

