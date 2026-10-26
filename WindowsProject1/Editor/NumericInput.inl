std::wstring GetNumericPrefix(const Block& block, int& field)
{
    field = 0;

    switch (block.type)
    {
    case BlockType::Move:
        field = 1;
        return L"前進する ";

    case BlockType::MoveBackward:
        field = 1;
        return L"後退する ";

    case BlockType::MoveRight:
        field = 1;
        return L"右に移動 ";

    case BlockType::MoveLeft:
        field = 1;
        return L"左に移動 ";

    case BlockType::TurnRight:
        field = 6;
        return L"右に旋回 ";

    case BlockType::TurnLeft:
        field = 6;
        return L"左に旋回 ";

    case BlockType::TurnGunRight:
        field = 6;
        return L"砲塔を右に旋回 ";

    case BlockType::TurnGunLeft:
        field = 6;
        return L"砲塔を左に旋回 ";

    case BlockType::TurnRadarRight:
        field = 6;
        return L"レーダーを右に旋回 ";

    case BlockType::TurnRadarLeft:
        field = 6;
        return L"レーダーを左に旋回 ";

    case BlockType::Repeat:
        field = 2;
        return L"繰り返す ";

    case BlockType::Wait:
        field = 3;
        return L"指定秒数待つ ";

    case BlockType::FireBullet:
        field = 7;
        return L"弾を撃つ 威力 ";

    case BlockType::CreateVariable:
        field = 5;
        return L"変数をつくる " + block.variableName + L" = ";

    case BlockType::SetVariable:
        field = 5;
        return block.variableName + L" に ";

    case BlockType::ChangeVariable:
        field = 5;
        return block.variableName + L" を ";

    case BlockType::AddVariable:
        field = 5;
        return block.variableName + L" に ";

    case BlockType::SubtractVariable:
        field = 5;
        return block.variableName + L" から ";

    case BlockType::If:
        field = 4;
        return L"もし条件値が ";

    default:
        return L"";
    }
}

LRESULT CALLBACK NumericEditProc(HWND edit, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_KEYDOWN && (wp == VK_RETURN || wp == VK_ESCAPE))
    {
        HWND parent = GetParent(edit);
        if (wp == VK_RETURN)
            SendMessageW(parent, WM_COMMAND, MAKEWPARAM(1090, EN_KILLFOCUS), reinterpret_cast<LPARAM>(edit));
        else
        {
            DestroyWindow(edit);
            g_numericEdit = nullptr;
            g_numericEditBlockIndex = -1;
            g_oldNumericEditProc = nullptr;
            SetFocus(parent);
            InvalidateRect(parent, nullptr, FALSE);
        }
        return 0;
    }
    return CallWindowProcW(g_oldNumericEditProc, edit, msg, wp, lp);
}

void FinishNumericEdit(HWND hwnd, bool commit)
{
    if (!g_numericEdit)
        return;

    if (commit && g_numericEditBlockIndex >= 0)
    {
        wchar_t buffer[64]{};
        GetWindowTextW(g_numericEdit, buffer, 63);
        wchar_t* end = nullptr;
        const auto& blocks = ActiveManager().GetBlocks();
        if (g_numericEditBlockIndex < static_cast<int>(blocks.size()))
        {
            Block updated = blocks[g_numericEditBlockIndex];

            if (g_numericEditField == 3)
            {
                // 待機時間：小数対応
                double value = wcstod(buffer, &end);

                if (end != buffer &&
                    *end == L'\0' &&
                    std::isfinite(value) &&
                    value >= 0.0 &&
                    value <= 100000.0)
                {
                    updated.waitSeconds = value;
                }
            }
            else if (g_numericEditField == 6)
            {
                // 旋回角度：degrees に保存
                double value = wcstod(buffer, &end);

                if (end != buffer &&
                    *end == L'\0' &&
                    std::isfinite(value) &&
                    value >= 0.0 &&
                    value <= 360000.0)
                {
                    updated.degrees = value;
                }
            }
            else if (g_numericEditField == 7)
            {
                // 弾の威力：power に保存
                double value = wcstod(buffer, &end);

                if (end != buffer &&
                    *end == L'\0' &&
                    std::isfinite(value) &&
                    value >= 0.0 &&
                    value <= 100000.0)
                {
                    updated.power = value;
                }
            }
            else
            {
                long value = wcstol(buffer, &end, 10);

                if (end != buffer &&
                    *end == L'\0' &&
                    value >= 0 &&
                    value <= 1000000)
                {
                    if (g_numericEditField == 1)
                        updated.distance = static_cast<int>(value);
                    else if (g_numericEditField == 2)
                        updated.repeatCount = static_cast<int>(value);
                    else if (g_numericEditField == 4)
                        updated.conditionValue = static_cast<int>(value);
                    else if (g_numericEditField == 5)
                        updated.value = static_cast<int>(value);
                }
            }


            ActiveManager().UpdateBlockAt(g_numericEditBlockIndex, updated);
        }
    }

    HWND edit = g_numericEdit;
    g_numericEdit = nullptr;
    g_numericEditBlockIndex = -1;
    g_numericEditField = 0;
    g_oldNumericEditProc = nullptr;
    if (IsWindow(edit)) DestroyWindow(edit);
    SetFocus(hwnd);
    InvalidateRect(hwnd, nullptr, FALSE);
}

// ============================================================
// 色変更ブロックの色見本部分をクリックしたか判定
// ============================================================

