LRESULT CALLBACK BlockListProc(
    HWND hwnd,
    UINT msg,
    WPARAM wp,
    LPARAM lp)
{
    if (!g_oldListProc)
    {
        return DefWindowProcW(hwnd, msg, wp, lp);
    }

    // まずリストボックス本来の処理を行う
    LRESULT result = CallWindowProcW(
        g_oldListProc,
        hwnd,
        msg,
        wp,
        lp
    );

    switch (msg)
    {

    case WM_MOUSEMOVE:
    {
        // マウスカーソルが乗っている項目を取得
        POINT p = {
            GET_X_LPARAM(lp),
            GET_Y_LPARAM(lp)
        };

        LRESULT hit = SendMessageW(
            hwnd,
            LB_ITEMFROMPOINT,
            0,
            MAKELPARAM(p.x, p.y)
        );

        int index = -1;

        // リストの外側では項目をホバー扱いにしない
        if (HIWORD(hit) == 0)
        {
            int itemIndex = LOWORD(hit);

            if (itemIndex >= 0 &&
                itemIndex < static_cast<int>(
                    g_visibleBlockTypes.size()))
            {
                index = itemIndex;
            }
        }

        // ホバー対象が変わった場合だけ再描画
        if (!g_draggingFromList &&
            g_hoveredBlockListItem != index)
        {
            g_hoveredBlockListItem = index;
            InvalidateRect(hwnd, nullptr, FALSE);
        }

        // マウスがリストから出たことを検知する
        TRACKMOUSEEVENT tme{};
        tme.cbSize = sizeof(tme);
        tme.dwFlags = TME_LEAVE;
        tme.hwndTrack = hwnd;

        TrackMouseEvent(&tme);

        break;
    }

    case WM_MOUSELEAVE:
    {
        // カーソルがリストから出たら解除
        if (g_hoveredBlockListItem != -1)
        {
            g_hoveredBlockListItem = -1;

            InvalidateRect(hwnd, nullptr, FALSE);
        }

        break;
    }

    case WM_LBUTTONDOWN:
    {
        POINT p = {
            GET_X_LPARAM(lp),
            GET_Y_LPARAM(lp)
        };

        RECT rc;
        GetClientRect(hwnd, &rc);

        // リストが開いていて、かつ表示されているときのみ右端でのリサイズを許可
        if (g_showBlockList &&
            g_blockList &&
            IsWindowVisible(g_blockList) &&
            abs(p.x - rc.right) <= 6)
        {
            g_isResizingList = true;
            SetCapture(g_mainWindow);
            return 0;
        }

        // クリックされた項目を取得
        LRESULT index = SendMessageW(
            hwnd,
            LB_GETCURSEL,
            0,
            0
        );

        if (index != LB_ERR &&
            index >= 0 &&
            index < static_cast<LRESULT>(
                g_visibleBlockTypes.size()))
        {
            g_dragBlockType =
                g_visibleBlockTypes[static_cast<size_t>(index)];

            g_dragBlockFunctionName.clear();

            if (index < static_cast<LRESULT>(
                g_visibleFunctionNames.size()))
            {
                g_dragBlockFunctionName =
                    g_visibleFunctionNames[
                        static_cast<size_t>(index)];
            }

            g_draggingFromList = true;

            // リストボックス座標からメインウィンドウ座標へ変換
            POINT dragPoint = {
                GET_X_LPARAM(lp),
                GET_Y_LPARAM(lp)
            };

            ClientToScreen(hwnd, &dragPoint);
            ScreenToClient(g_mainWindow, &dragPoint);

            g_listDragPoint = dragPoint;

            // ドラッグ中のマウス操作をメインウィンドウで受け取る
            SetCapture(g_mainWindow);

            if (g_dragPreviewWnd)
            {
                POINT screenPoint = g_listDragPoint;
                ClientToScreen(g_mainWindow, &screenPoint);

                SetWindowPos(
                    g_dragPreviewWnd,
                    HWND_TOPMOST,
                    screenPoint.x - 84,
                    screenPoint.y - 15,
                    168,
                    30,
                    SWP_NOACTIVATE | SWP_SHOWWINDOW
                );
            }
        }
        break;
    }
    }

    return result;
}





// ============================================================
// カテゴリボタンを作成
// ============================================================
    LRESULT CALLBACK CategoryButtonProc(
        HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
    {
        int index = -1;

        for (int i = 0; i < CATEGORY_COUNT; ++i)
        {
            if (g_categoryButtons[i] == hwnd)
            {
                index = i;
                break;
            }
        }

        if (index < 0 || !g_oldCategoryProcs[index])
        {
            return DefWindowProcW(hwnd, msg, wp, lp);
        }

        switch (msg)
        {
        case WM_MOUSEMOVE:
        {
            if (g_hoveredCategory != index)
            {
                int previousCategory = g_hoveredCategory;

                g_hoveredCategory = index;

                // 前のボタンの光を消す
                if (previousCategory >= 0 &&
                    previousCategory < CATEGORY_COUNT &&
                    g_categoryButtons[previousCategory])
                {
                    InvalidateRect(
                        g_categoryButtons[previousCategory],
                        nullptr,
                        TRUE);
                }

                // 現在のボタンを再描画
                InvalidateRect(hwnd, nullptr, TRUE);
            }

            TRACKMOUSEEVENT tme{};
            tme.cbSize = sizeof(tme);
            tme.dwFlags = TME_LEAVE;
            tme.hwndTrack = hwnd;

            TrackMouseEvent(&tme);
            break;
        }

        case WM_MOUSELEAVE:
        {
            if (g_hoveredCategory == index)
            {
                g_hoveredCategory = -1;

                // 光を消して再描画
                InvalidateRect(hwnd, nullptr, TRUE);
            }
            break;
        }
        }

        return CallWindowProcW(
            g_oldCategoryProcs[index],
            hwnd,
            msg,
            wp,
            lp);
}



void CreateCategoryButtons(HWND hwnd)
{
    constexpr int BUTTON_X = 12;
    constexpr int BUTTON_Y = 95;
    constexpr int BUTTON_WIDTH = 202;
    constexpr int BUTTON_HEIGHT = 38;
    constexpr int BUTTON_GAP = 6;

    for (int i = 0; i < CATEGORY_COUNT; ++i)
    {
        g_categoryButtons[i] = CreateWindowW(
            L"BUTTON",
            CATEGORY_NAMES[i],
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            BUTTON_X,
            BUTTON_Y + i * (BUTTON_HEIGHT + BUTTON_GAP),
            BUTTON_WIDTH,
            BUTTON_HEIGHT,
            hwnd,
            reinterpret_cast<HMENU>(
                static_cast<INT_PTR>(
                    ID_CATEGORY_BASE + i
                    )
                ),
            GetModuleHandleW(nullptr),
            nullptr
        );

        if (g_categoryButtons[i])
        {
            g_oldCategoryProcs[i] = reinterpret_cast<WNDPROC>(
                SetWindowLongPtrW(
                    g_categoryButtons[i],
                    GWLP_WNDPROC,
                    reinterpret_cast<LONG_PTR>(CategoryButtonProc)
                )
                );
        }

    }
}

// ============================================================
// カテゴリボタンの描画
// ============================================================

