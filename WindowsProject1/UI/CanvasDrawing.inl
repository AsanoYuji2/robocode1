void DrawPanel(
    HDC hdc,
    int left,
    int top,
    int right,
    int bottom,
    COLORREF color)
{
    RECT rc{ left, top, right, bottom };

    HBRUSH brush = CreateSolidBrush(color);
    FillRect(hdc, &rc, brush);
    DeleteObject(brush);
}

void DrawLabel(
    HDC hdc,
    const wchar_t* text,
    RECT rc,
    COLORREF color,
    int fontSize,
    bool bold = false)
{
    HFONT font = CreateFontW(
        -fontSize,
        0,
        0,
        0,
        bold ? FW_BOLD : FW_NORMAL,
        FALSE,
        FALSE,
        FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        L"Meiryo UI"
    );

    HFONT oldFont =
        (HFONT)SelectObject(hdc, font);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, color);

    DrawTextW(
        hdc,
        text,
        -1,
        &rc,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE
    );

    SelectObject(hdc, oldFont);
    DeleteObject(font);
}

// ============================================================
// 上部ツールバーのボタン描画
// ============================================================
void DrawToolbarButton(
    HDC hdc,
    int x,
    int y,
    int width,
    int height,
    const wchar_t* text,
    COLORREF color)
{
    RECT rc = {
        x,
        y,
        x + width,
        y + height
    };

    HBRUSH brush = CreateSolidBrush(color);
    FillRect(hdc, &rc, brush);
    DeleteObject(brush);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(255, 255, 255));

    DrawTextW(
        hdc,
        text,
        -1,
        &rc,
        DT_CENTER | DT_VCENTER | DT_SINGLELINE
    );
}

// ============================================================
// キャンバスのドット背景
// ============================================================
void DrawCanvasGrid(
    HDC hdc,
    const RECT& canvasRect)
{
    const int gridSize = 20;

    HPEN pen = CreatePen(
        PS_SOLID,
        1,
        RGB(218, 225, 235)
    );

    HPEN oldPen = (HPEN)SelectObject(hdc, pen);

    // 20ピクセル間隔で点を描画
    for (
        int y = canvasRect.top + 10;
        y < canvasRect.bottom;
        y += gridSize)
    {
        for (
            int x = canvasRect.left + 10;
            x < canvasRect.right;
            x += gridSize)
        {
            SetPixelV(
                hdc,
                x,
                y,
                RGB(195, 207, 222)
            );
        }
    }

    SelectObject(hdc, oldPen);
    DeleteObject(pen);
}



// ファイルから安全に GDI+ Bitmap を読み込むヘルパー関数
Bitmap* LoadBitmapFromExeFolder(const wchar_t* filename)
{
    // 1. 実行ファイル（.exe）のフルパスを取得する
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);

    // 2. ファイル名部分を削って、フォルダのパスだけにする
    wchar_t* lastSlash = wcsrchr(exePath, L'\\');
    if (lastSlash) {
        *(lastSlash + 1) = L'\0'; // 例: "C:\...\Debug\"
    }

    // 3. フォルダパスとファイル名を結合する (例: "C:\...\Debug\new.png")
    std::wstring fullPath = std::wstring(exePath) + filename;

    // 4. ファイルを開く
    HANDLE hFile = CreateFileW(fullPath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return nullptr;

    DWORD fileSize = GetFileSize(hFile, nullptr);
    if (fileSize == INVALID_FILE_SIZE) {
        CloseHandle(hFile);
        return nullptr;
    }

    // 5. メモリに読み込む
    HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE, fileSize);
    if (!hGlobal) {
        CloseHandle(hFile);
        return nullptr;
    }

    void* pBuffer = GlobalLock(hGlobal);
    DWORD bytesRead = 0;
    ReadFile(hFile, pBuffer, fileSize, &bytesRead, nullptr);
    GlobalUnlock(hGlobal);
    CloseHandle(hFile);

    // 6. GDI+ の Bitmap を生成
    IStream* pStream = nullptr;
    if (CreateStreamOnHGlobal(hGlobal, TRUE, &pStream) != S_OK) {
        GlobalFree(hGlobal);
        return nullptr;
    }

    Bitmap* pBmp = Bitmap::FromStream(pStream);
    pStream->Release();

    if (!pBmp || pBmp->GetLastStatus() != Ok) {
        delete pBmp;
        return nullptr;
    }

    return pBmp;
}

void DrawNewLayout(HDC hdc, int width, int height)
{
    int leftPanelWidth = g_canvasLeft;
    int rightPanelWidth = 0;
    int canvasLeft = leftPanelWidth;
    int canvasRight = width - rightPanelWidth;

    //全体の背景
    DrawPanel(hdc, 0, 0, width, height, RGB(235, 240, 247));

    //上部ツールバー
    DrawPanel(hdc, 0, 0, width, 50, RGB(35, 43, 58));

    RECT titleRect{ 16, 0, 230, 50 };
    DrawLabel(
        hdc,
        L"Robocode Block Editor",
        titleRect,
        RGB(255, 255, 255),
        18,
        true
    );

    //左側パネル
    DrawPanel(hdc, 0, 50, leftPanelWidth, height, RGB(247, 249, 252));

    RECT categoryRect{ 12, 50, leftPanelWidth - 10, 85 };
    DrawLabel(
        hdc,
        L"ブロックカテゴリ",
        categoryRect,
        RGB(45, 55, 72),
        16,
        true
    );

    //キャンバス背景
    DrawPanel(hdc, canvasLeft, 50, canvasRight, height, RGB(250, 252, 255));

    HPEN pen = CreatePen(PS_SOLID, 1, RGB(205, 215, 228));
    HPEN oldPen = (HPEN)SelectObject(hdc, pen);

    MoveToEx(hdc, canvasLeft, 50, nullptr);
    LineTo(hdc, canvasLeft, height);

    //右側の境界線をウィンドウ右端に合わせる
    MoveToEx(hdc, canvasRight, 50, nullptr);
    LineTo(hdc, canvasRight, height);

    SelectObject(hdc, oldPen);
    DeleteObject(pen);

}

// フォルダ選択ダイアログの初期フォルダを指定するためのコールバック
