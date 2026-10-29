bool IsColorEditArea(const Block& block, POINT worldPoint)
{
    // ブロック右端の色見本部分をクリック範囲にする
    constexpr int COLOR_AREA_WIDTH = 32;

    int left = block.x + block.w - COLOR_AREA_WIDTH;
    int right = block.x + block.w;

    return
        worldPoint.x >= left &&
        worldPoint.x < right &&
        worldPoint.y >= block.y &&
        worldPoint.y < block.y + block.h;
}
bool TryStartColorEdit(HWND hwnd, POINT worldPoint)
{
    const auto& blocks = ActiveManager().GetBlocks();

    // 手前のブロックから判定する
    for (int i = static_cast<int>(blocks.size()) - 1;
        i >= 0; --i)
    {
        const Block& block = blocks[static_cast<size_t>(i)];

        // ブロック内で、さらに色選択専用の範囲内か判定
        if (!IsColorEditArea(block, worldPoint))
        {
            continue;
        }

        COLORREF currentColor = RGB(0, 0, 0);

        switch (block.type)
        {
        case BlockType::SetBodyColor:
            currentColor = block.bodyColor;
            break;

        case BlockType::SetGunColor:
            currentColor = block.gunColor;
            break;

        case BlockType::SetRadarColor:
            currentColor = block.radarColor;
            break;

        case BlockType::SetBulletColor:
            currentColor = block.bulletColor;
            break;

        case BlockType::SetScanColor:
            currentColor = block.scanColor;
            break;

        default:
            return false;
        }

        // 色選択ダイアログを開く
        static COLORREF customColors[16]{};

        CHOOSECOLORW cc{};
        cc.lStructSize = sizeof(cc);
        cc.hwndOwner = hwnd;
        cc.rgbResult = currentColor;
        cc.lpCustColors = customColors;
        cc.Flags = CC_FULLOPEN | CC_RGBINIT;

        if (ChooseColorW(&cc))
        {
            // 選択結果をブロックに反映
            Block updated = block;

            switch (block.type)
            {
            case BlockType::SetBodyColor:
                updated.bodyColor = cc.rgbResult;
                break;

            case BlockType::SetGunColor:
                updated.gunColor = cc.rgbResult;
                break;

            case BlockType::SetRadarColor:
                updated.radarColor = cc.rgbResult;
                break;

            case BlockType::SetBulletColor:
                updated.bulletColor = cc.rgbResult;
                break;

            case BlockType::SetScanColor:
                updated.scanColor = cc.rgbResult;
                break;

            default:
                break;
            }

            ActiveManager().UpdateBlockAt(i, updated);
            InvalidateEditor();
        }

        return true;
    }

    return false;
}

bool TryStartNumericEdit(HWND hwnd, POINT screenPoint, POINT worldPoint)
{
    const auto& blocks = ActiveManager().GetBlocks();
    for (int i = static_cast<int>(blocks.size()) - 1; i >= 0; --i)
    {
        
        const Block& block = blocks[static_cast<size_t>(i)];

        
        // 「もし常に真なら」には数値入力欄を表示しない
        if (block.type == BlockType::If &&
            block.condition == ConditionType::Always)
        {
            continue;
        }

        

        int field = 0;
        std::wstring prefix = GetNumericPrefix(block, field);
        if (field == 0 || worldPoint.y < block.y + 8 || worldPoint.y > block.y + block.h - 4)
            continue;

        HDC hdc = GetDC(hwnd);
        HFONT font = reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
        HGDIOBJ oldFont = SelectObject(hdc, font);
        SIZE prefixSize{};
        GetTextExtentPoint32W(hdc, prefix.c_str(), static_cast<int>(prefix.size()), &prefixSize);
        SelectObject(hdc, oldFont);
        ReleaseDC(hwnd, hdc);

        int valueX = block.x + 12 + prefixSize.cx;
        int valueWidth = 54;
        int left = static_cast<int>(std::lround(valueX * g_zoom + g_viewX));
        int top = static_cast<int>(std::lround((block.y + 15) * g_zoom + g_viewY));
        int width = static_cast<int>(std::lround(valueWidth * g_zoom));
        int height = static_cast<int>(std::lround(24 * g_zoom));
        RECT hit{ left - 3, top - 3, left + width + 3, top + height + 3 };
        if (screenPoint.x < hit.left || screenPoint.x > hit.right || screenPoint.y < hit.top || screenPoint.y > hit.bottom)
            continue;

        if (g_numericEdit) FinishNumericEdit(hwnd, true);
        std::wstring initial;

        if (field == 1)
            initial = std::to_wstring(block.distance);
        else if (field == 2)
            initial = std::to_wstring(block.repeatCount);
        else if (field == 3)
            initial = std::to_wstring(block.waitSeconds);
        else if (field == 6)
        {
            // 旋回角度：小数点以下の不要な0を削除
            wchar_t buffer[64]{};
            swprintf_s(buffer, L"%.10g", block.degrees);
            initial = buffer;
        }
        else if (field == 7)
        {
            // 弾の威力：小数点以下の不要な0を削除
            wchar_t buffer[64]{};
            swprintf_s(buffer, L"%.10g", block.power);
            initial = buffer;
        }
        else if (field == 4)
            initial = std::to_wstring(block.conditionValue);
        else
            initial = std::to_wstring(block.value);

        g_numericEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", initial.c_str(),
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_LEFT,
            left, top, (std::max)(width, 44), (std::max)(height, 20), hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(1090)), GetModuleHandleW(nullptr), nullptr);
        if (!g_numericEdit) return false;
        g_numericEditBlockIndex = i;
        g_numericEditField = field;
        g_oldNumericEditProc = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(g_numericEdit, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(NumericEditProc)));
        SendMessageW(g_numericEdit, EM_SETSEL, 0, -1);
        SetFocus(g_numericEdit);
        return true;
    }
    return false;
}



// ============================================================
// 新しい画面レイアウト用の描画処理
// Main.cpp の WndProc より上に追加
// ============================================================

