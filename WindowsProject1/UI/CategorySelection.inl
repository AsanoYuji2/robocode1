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
        InvalidateRect(GetActiveWindow(), nullptr, TRUE);
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
            int targetCanvasLeft = 220 + g_listWidth + 20;
            int maxCanvasLeft = clientRc.right - 340;
            if (targetCanvasLeft > maxCanvasLeft) targetCanvasLeft = maxCanvasLeft;
            if (targetCanvasLeft < 250) targetCanvasLeft = 250;

            g_canvasLeft = targetCanvasLeft;
            int delta = g_canvasLeft - oldLeft;
            g_viewX += delta;

            // ==========================================
            // 【完全版バックアップ＆平行移動方式】
            // ==========================================
            auto blocks = ActiveManager().GetBlocks();

            // 1. 初めて開く直前のX座標をバックアップ
            if (g_originalXMap.empty())
            {
                for (size_t i = 0; i < blocks.size(); ++i)
                {
                    g_originalXMap[i] = blocks[i].x;
                }
            }

            // 2. リストの右端から必要な余白（例: 45px）を空けた目標ライン
            int listScreenRight = ptTopLeft.x + g_listWidth;
            POINT worldListRight = ActiveManager().ScreenToWorld(listScreenRight, CANVAS_TOP);
            int targetMinX = worldListRight.x + 45;

            // 3. 一番左にあるブロックを基準に、必要な全体のスライド量を求める
            int maxShiftNeeded = 0;
            for (const auto& pair : g_originalXMap)
            {
                int origX = pair.second;
                if (origX < targetMinX)
                {
                    int needed = targetMinX - origX;
                    if (needed > maxShiftNeeded)
                    {
                        maxShiftNeeded = needed;
                    }
                }
            }

            // 4. 各ブロックの「初期位置 ＋ スライド量」に配置する（個別計算で重なりを防ぐ）
            if (maxShiftNeeded > 0)
            {
                for (size_t i = 0; i < blocks.size(); ++i)
                {
                    if (g_originalXMap.find(i) != g_originalXMap.end())
                    {
                        int origX = g_originalXMap[i];
                        Block b = blocks[i];

                        // リスト周辺に位置していたブロックなら、初期位置をベースに右へ押し出す
                        if (origX < targetMinX + 250)
                        {
                            b.x = origX + maxShiftNeeded;
                        }
                        else
                        {
                            b.x = origX; // 遠くのブロックは初期位置のまま
                        }

                        blocks[i] = b;
                        ActiveManager().UpdateBlockAt(static_cast<int>(i), b);
                    }
                }
            }

            auto updatedBlocks = ActiveManager().GetBlocks();
            g_originalXMap.clear();
            for (size_t i = 0; i < updatedBlocks.size(); ++i)
            {
                g_originalXMap[i] = updatedBlocks[i].x;
            }
            // ==========================================

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


