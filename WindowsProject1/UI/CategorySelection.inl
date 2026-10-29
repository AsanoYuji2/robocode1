

void SelectCategory(BlockCategory category)
{

    if (category < BlockCategory::Move ||
        category >= BlockCategory::Count)
    {
        return;
    }

    // すでに同じカテゴリが選択されている状態でクリックされた場合は、リストを閉じて終了する（トグル動作）
    if (g_selectedCategory == category && g_blockList && IsWindowVisible(g_blockList))
    {
        CloseBlockPalette(); // 閉じる処理とブロック復元はすべてここで実行
        InvalidateRect(GetActiveWindow(), nullptr, FALSE);
        return;
    }

    // すでに別のリストが開いている状態かどうかを判定
    bool isOpen = (g_blockList && IsWindowVisible(g_blockList));

    g_selectedCategory = category;
    InitializeBlockList(g_blockList);

    if (g_categoryButtons[0] && g_blockList)
    {
        RECT btnRc{};
        GetWindowRect(g_tabFuncBtn, &btnRc);

        POINT ptTopLeft = { btnRc.right + 5, btnRc.top };
        ScreenToClient(g_mainWindow, &ptTopLeft);

        RECT clientRc{};
        GetClientRect(g_mainWindow, &clientRc);
        int maxListWidth = clientRc.right - 350;
        if (maxListWidth < 120) maxListWidth = 120;
        if (g_listWidth > maxListWidth) g_listWidth = maxListWidth;

        SetWindowPos(
            g_blockList,
            HWND_TOP,
            ptTopLeft.x,
            ptTopLeft.y,
            g_listWidth,
            475,
            SWP_SHOWWINDOW
        );

        if (!isOpen)
        {
            // 初めて開く場合
            int oldLeft = g_canvasLeft;
            int targetCanvasLeft = 220 + g_listWidth + 10;
            int maxCanvasLeft = clientRc.right - 340;
            if (targetCanvasLeft > maxCanvasLeft) targetCanvasLeft = maxCanvasLeft;
            if (targetCanvasLeft < 250) targetCanvasLeft = 250;

            g_canvasLeft = targetCanvasLeft;
            int delta = g_canvasLeft - oldLeft;
            g_viewX += delta;

            auto blocks = ActiveManager().GetBlocks();

            int listScreenRight = ptTopLeft.x + g_listWidth;
            POINT worldListRight = ActiveManager().ScreenToWorld(listScreenRight, CANVAS_TOP);
            int targetMinX = worldListRight.x + 5;

            // 一番左にあるブロックを基準に、必要な全体のスライド量を求める
            int maxShiftNeeded = 0;
            for (const auto& b : blocks)
            {
                if (b.x < targetMinX && b.x > targetMinX - 80)
                {
                    int needed = targetMinX - b.x;
                    if (needed > maxShiftNeeded)
                    {
                        maxShiftNeeded = needed;
                    }
                }
            }

            // 移動量をグローバル変数に記憶！
            g_totalShiftX = maxShiftNeeded;

            // 各ブロックを右へ押し出す
            if (g_totalShiftX > 0)
            {
                for (size_t i = 0; i < blocks.size(); ++i)
                {
                    Block b = blocks[i];
                    if (b.x < targetMinX + 250)
                    {
                        b.x += g_totalShiftX;
                    }
                    blocks[i] = b;
                    ActiveManager().UpdateBlockAt(static_cast<int>(i), b);
                }
            }

            UpdateView();
        }
        else
        {
            // すでに開いている状態で別のカテゴリに変えた場合
            int targetCanvasLeft = 220 + g_listWidth + 10;
            int maxCanvasLeft = clientRc.right - 340;
            if (targetCanvasLeft > maxCanvasLeft) targetCanvasLeft = maxCanvasLeft;
            if (targetCanvasLeft < 250) targetCanvasLeft = 250;

            int oldLeft = g_canvasLeft;
            g_canvasLeft = targetCanvasLeft;
            int delta = g_canvasLeft - oldLeft;
            g_viewX += delta;
            UpdateView();
        }
    }

    g_showBlockList = true;

    if (g_deleteMode)
    {
        g_deleteMode = false;
        ActiveManager().SetDeleteMode(false);
    }

    UpdateButtonColors();
    InvalidateEditor();

    if (g_mainWindow)
        SetFocus(g_mainWindow);
}

// ============================================================
// 作業エリア内か判定
// ============================================================

bool IsCanvasPoint(POINT p)
{
    if (!g_mainWindow)
        return false;

    RECT client;
    GetClientRect(g_mainWindow, &client);

    return
        p.x >= g_canvasLeft &&
        p.y >= CANVAS_TOP &&
        p.x < client.right &&
        p.y < client.bottom;
}

bool IsBlackArea(POINT p)
{
    RECT client{};
    GetClientRect(g_mainWindow, &client);

    RECT canvasRect = {
        g_canvasLeft,
        CANVAS_TOP,
        client.right - 20,
        client.bottom - 20
    };

    return !PtInRect(&canvasRect, p);
}

// ============================================================
// ブロック一覧からドラッグ＆ドロップ
// ============================================================


