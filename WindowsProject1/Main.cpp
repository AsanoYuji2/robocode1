#include <windows.h>
#include <windowsx.h>
#include <cstdlib>
#include <cmath>
#include <string>
#include <algorithm>
#include <cwchar>

#include "BlockManager.h"
#include "JavaGenerator.h"

#include <commdlg.h>
#include "BlockFileManager.h"

#pragma comment(lib, "Comdlg32.lib")


BlockManager g_manager;

HWND g_blockList = nullptr;
HWND g_addButton = nullptr;
HWND g_deleteButton = nullptr;
HWND g_resetViewButton = nullptr;
HWND g_mainWindow = nullptr;

WNDPROC g_oldListProc = nullptr;

bool g_deleteMode = false;
bool g_showBlockList = false;
bool g_draggingFromList = false;
bool g_panning = false;

BlockType g_dragBlockType = BlockType::Move;

POINT g_dragStart = { 0, 0 };
POINT g_lastPanPoint = { 0, 0 };

float g_zoom = 1.0f;
float g_viewX = 350.0f;
float g_viewY = 0.0f;

constexpr float MIN_ZOOM = 0.25f;
constexpr float MAX_ZOOM = 3.0f;

constexpr float INITIAL_ZOOM = 1.0f;
constexpr float INITIAL_VIEW_X = 350.0f;
constexpr float INITIAL_VIEW_Y = 0.0f;

constexpr int CANVAS_LEFT = 420;
constexpr int CANVAS_TOP = 50;

constexpr int ID_JAVA = 1001;
constexpr int ID_ADD = 1002;
constexpr int ID_DELETE = 1003;
constexpr int ID_BLOCK_LIST = 1004;
constexpr int ID_RESET_VIEW = 1005;
constexpr int ID_SAVE = 1006;
constexpr int ID_LOAD = 1007;

const COLORREF COLOR_NORMAL = RGB(65, 65, 75);
const COLORREF COLOR_ADD_ON = RGB(40, 120, 220);
const COLORREF COLOR_DELETE_ON = RGB(220, 65, 65);


// 座標変換を更新
void UpdateView()
{
    g_zoom = (std::max)(
        MIN_ZOOM,
        (std::min)(g_zoom, MAX_ZOOM)
        );

    g_manager.SetViewTransform(
        g_zoom,
        g_viewX,
        g_viewY
    );
}


// リスト番号からブロック種類を取得
BlockType GetBlockTypeFromIndex(int index)
{
    switch (index)
    {
    case 0: return BlockType::Move;
    case 1: return BlockType::Turn;
    case 2: return BlockType::TurnLeft;
    case 3: return BlockType::MoveRight;
    case 4: return BlockType::MoveLeft;
    case 5: return BlockType::Fire;
    case 6: return BlockType::Bark;
    default: return BlockType::Move;
    }
}


// ブロック一覧を初期化
void InitializeBlockList(HWND list)
{
    SendMessageW(list, LB_RESETCONTENT, 0, 0);

    const BlockType types[] =
    {
        BlockType::Move,
        BlockType::Turn,
        BlockType::TurnLeft,
        BlockType::MoveRight,
        BlockType::MoveLeft,
        BlockType::Fire,
        BlockType::Bark
    };

    for (BlockType type : types)
    {
        Block block{};
        block.type = type;

        std::wstring name = GetBlockName(block);

        SendMessageW(
            list,
            LB_ADDSTRING,
            0,
            reinterpret_cast<LPARAM>(name.c_str())
        );
    }

    SendMessageW(
        list,
        LB_SETCURSEL,
        static_cast<WPARAM>(-1),
        0
    );
}


// ボタンを再描画
void UpdateButtonColors()
{
    if (g_addButton)
        InvalidateRect(g_addButton, nullptr, FALSE);

    if (g_deleteButton)
        InvalidateRect(g_deleteButton, nullptr, FALSE);
}


// ボタンの独自描画
void DrawCustomButton(
    const DRAWITEMSTRUCT* dis,
    COLORREF color,
    const wchar_t* text)
{
    HDC hdc = dis->hDC;
    RECT rc = dis->rcItem;

    HBRUSH brush = CreateSolidBrush(color);
    FillRect(hdc, &rc, brush);
    DeleteObject(brush);

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


// 作業エリア内か判定
bool IsCanvasPoint(POINT p)
{
    RECT client;
    GetClientRect(g_mainWindow, &client);

    return
        p.x >= CANVAS_LEFT &&
        p.y >= CANVAS_TOP &&
        p.x < client.right &&
        p.y < client.bottom;
}


// リストからのドラッグ＆ドロップ
LRESULT CALLBACK BlockListProc(
    HWND hwnd,
    UINT msg,
    WPARAM wp,
    LPARAM lp)
{
    int x = 0;
    int y = 0;

    switch (msg)
    {
    case WM_LBUTTONDOWN:
    {
        LRESULT result = CallWindowProcW(
            g_oldListProc,
            hwnd,
            msg,
            wp,
            lp
        );

        LRESULT itemInfo = SendMessageW(
            hwnd,
            LB_ITEMFROMPOINT,
            0,
            MAKELPARAM(
                GET_X_LPARAM(lp),
                GET_Y_LPARAM(lp)
            )
        );

        if (HIWORD(itemInfo) == 0)
        {
            int index = LOWORD(itemInfo);

            if (index >= 0 && index < 7)
            {
                g_dragBlockType =
                    GetBlockTypeFromIndex(index);

                g_dragStart.x = GET_X_LPARAM(lp);
                g_dragStart.y = GET_Y_LPARAM(lp);

                g_draggingFromList = false;

                SetCapture(hwnd);
            }
        }

        return result;
    }

    case WM_MOUSEMOVE:
    {
        x = GET_X_LPARAM(lp);
        y = GET_Y_LPARAM(lp);

        // リストからのドラッグ開始判定
        if (GetCapture() == hwnd && (wp & MK_LBUTTON))
        {
            int dx = x - g_dragStart.x;
            int dy = y - g_dragStart.y;

            if (dx * dx + dy * dy >= 16)
            {
                g_draggingFromList = true;
            }
        }

        return 0;
    }

    case WM_LBUTTONUP:
    {
        if (GetCapture() == hwnd)
        {
            POINT dropPoint =
            {
                GET_X_LPARAM(lp),
                GET_Y_LPARAM(lp)
            };

            ClientToScreen(hwnd, &dropPoint);
            ScreenToClient(g_mainWindow, &dropPoint);

            bool wasDragging = g_draggingFromList;
            g_draggingFromList = false;

            ReleaseCapture();

            if (
                wasDragging &&
                !g_deleteMode &&
                IsCanvasPoint(dropPoint)
                )
            {
                POINT world = g_manager.ScreenToWorld(
                    dropPoint.x,
                    dropPoint.y
                );

                g_manager.AddBlock(
                    g_dragBlockType,
                    world.x - 90,
                    world.y - 27
                );

                InvalidateRect(
                    g_mainWindow,
                    nullptr,
                    FALSE
                );
            }

            return CallWindowProcW(
                g_oldListProc,
                hwnd,
                msg,
                wp,
                lp
            );
        }

        break;
    }

    case WM_CAPTURECHANGED:
        g_draggingFromList = false;
        break;
    }

    return CallWindowProcW(
        g_oldListProc,
        hwnd,
        msg,
        wp,
        lp
    );
}

bool SaveProject(HWND hwnd)
{
    wchar_t path[MAX_PATH] = L"";

    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter =
        L"Block Editor Files (*.blk)\0*.blk\0"
        L"All Files (*.*)\0*.*\0";
    ofn.lpstrFile = path;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = L"blk";
    ofn.Flags =
        OFN_OVERWRITEPROMPT |
        OFN_PATHMUSTEXIST;

    if (!GetSaveFileNameW(&ofn))
        return false;

    return BlockFileManager::SaveToFile(
        path,
        g_manager.GetBlocks(),
        g_manager.GetParents()
    );
}

bool LoadProject(HWND hwnd)
{
    wchar_t path[MAX_PATH] = L"";

    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter =
        L"Block Editor Files (*.blk)\0*.blk\0"
        L"All Files (*.*)\0*.*\0";
    ofn.lpstrFile = path;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = L"blk";
    ofn.Flags =
        OFN_FILEMUSTEXIST |
        OFN_PATHMUSTEXIST;

    if (!GetOpenFileNameW(&ofn))
        return false;

    std::vector<Block> loadedBlocks;
    std::vector<int> loadedParents;

    if (!BlockFileManager::LoadFromFile(
        path,
        loadedBlocks,
        loadedParents))
    {
        return false;
    }

    if (!g_manager.ReplaceBlocks(
        loadedBlocks,
        loadedParents))
    {
        return false;
    }

    InvalidateRect(hwnd, nullptr, FALSE);
    return true;
}

// メインウィンドウの処理
LRESULT CALLBACK WndProc(
    HWND hwnd,
    UINT msg,
    WPARAM wp,
    LPARAM lp)
{
    int x = 0;
    int y = 0;

    if (
        msg == WM_LBUTTONDOWN ||
        msg == WM_LBUTTONUP ||
        msg == WM_MOUSEMOVE ||
        msg == WM_RBUTTONDOWN ||
        msg == WM_RBUTTONUP
        )
    {
        x = GET_X_LPARAM(lp);
        y = GET_Y_LPARAM(lp);
    }

    switch (msg)
    {
    case WM_CREATE:
    {
        g_mainWindow = hwnd;

        g_manager.Initialize();
        UpdateView();

        // ブロック追加ボタン
        g_addButton = CreateWindowW(
            L"BUTTON",
            L"ブロック追加",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            150, 10, 120, 38,
            hwnd,
            reinterpret_cast<HMENU>(
                static_cast<INT_PTR>(ID_ADD)
                ),
            GetModuleHandleW(nullptr),
            nullptr
        );

        // ブロック削除ボタン
        g_deleteButton = CreateWindowW(
            L"BUTTON",
            L"ブロック削除",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            280, 10, 120, 38,
            hwnd,
            reinterpret_cast<HMENU>(
                static_cast<INT_PTR>(ID_DELETE)
                ),
            GetModuleHandleW(nullptr),
            nullptr
        );

        // ズームリセットボタン
        g_resetViewButton = CreateWindowW(
            L"BUTTON",
            L"ズームリセット",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            410, 10, 120, 38,
            hwnd,
            reinterpret_cast<HMENU>(
                static_cast<INT_PTR>(ID_RESET_VIEW)
                ),
            GetModuleHandleW(nullptr),
            nullptr
        );

        // ブロック一覧
        g_blockList = CreateWindowW(
            L"LISTBOX",
            nullptr,
            WS_CHILD |
            LBS_NOTIFY |
            WS_BORDER |
            WS_VSCROLL,
            150, 50, 180, 110,
            hwnd,
            reinterpret_cast<HMENU>(
                static_cast<INT_PTR>(ID_BLOCK_LIST)
                ),
            GetModuleHandleW(nullptr),
            nullptr
        );

        CreateWindowW(
            L"BUTTON",
            L"保存",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            540, 10, 80, 38,
            hwnd,
            reinterpret_cast<HMENU>(
                static_cast<INT_PTR>(ID_SAVE)),
            GetModuleHandleW(nullptr),
            nullptr
        );

        CreateWindowW(
            L"BUTTON",
            L"読み込み",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            630, 10, 90, 38,
            hwnd,
            reinterpret_cast<HMENU>(
                static_cast<INT_PTR>(ID_LOAD)),
            GetModuleHandleW(nullptr),
            nullptr
        );

        InitializeBlockList(g_blockList);

        ShowWindow(g_blockList, SW_HIDE);

        g_oldListProc = reinterpret_cast<WNDPROC>(
            SetWindowLongPtrW(
                g_blockList,
                GWLP_WNDPROC,
                reinterpret_cast<LONG_PTR>(BlockListProc)
            )
            );

        return 0;
    }

    // 背景消去を抑制
    case WM_ERASEBKGND:
        return 1;

        // 追加・削除ボタンの描画
    case WM_DRAWITEM:
    {
        auto* dis =
            reinterpret_cast<DRAWITEMSTRUCT*>(lp);

        if (dis->CtlID == ID_ADD)
        {
            DrawCustomButton(
                dis,
                g_showBlockList
                ? COLOR_ADD_ON
                : COLOR_NORMAL,
                L"ブロック追加"
            );

            return TRUE;
        }

        if (dis->CtlID == ID_DELETE)
        {
            DrawCustomButton(
                dis,
                g_deleteMode
                ? COLOR_DELETE_ON
                : COLOR_NORMAL,
                L"ブロック削除"
            );

            return TRUE;
        }

        break;
    }

    case WM_COMMAND:
    {
        switch (LOWORD(wp))
        {
        case ID_ADD:
        {
            g_showBlockList = !g_showBlockList;

            if (g_showBlockList && g_deleteMode)
            {
                g_deleteMode = false;
                g_manager.SetDeleteMode(false);
            }

            ShowWindow(
                g_blockList,
                g_showBlockList ? SW_SHOW : SW_HIDE
            );

            SendMessageW(
                g_blockList,
                LB_SETCURSEL,
                static_cast<WPARAM>(-1),
                0
            );

            UpdateButtonColors();
            SetFocus(hwnd);

            return 0;
        }

        case ID_SAVE:
        {
            if (SaveProject(hwnd))
            {
                MessageBoxW(
                    hwnd,
                    L"プロジェクトを保存しました。",
                    L"保存完了",
                    MB_OK | MB_ICONINFORMATION
                );
            }
            else
            {
                // キャンセルと保存失敗を区別する場合は、
                // 関数の戻り値を別途設計してください。
                MessageBoxW(
                    hwnd,
                    L"保存できませんでした。",
                    L"保存エラー",
                    MB_OK | MB_ICONERROR
                );
            }

            return 0;
        }

        case ID_LOAD:
        {
            if (LoadProject(hwnd))
            {
                MessageBoxW(
                    hwnd,
                    L"プロジェクトを読み込みました。",
                    L"読み込み完了",
                    MB_OK | MB_ICONINFORMATION
                );
            }
            else
            {
                MessageBoxW(
                    hwnd,
                    L"読み込みできませんでした。",
                    L"読み込みエラー",
                    MB_OK | MB_ICONERROR
                );
            }

            return 0;
        }

        case ID_DELETE:
        {
            g_deleteMode = !g_deleteMode;

            g_manager.SetDeleteMode(g_deleteMode);

            if (g_deleteMode)
            {
                g_showBlockList = false;
                ShowWindow(g_blockList, SW_HIDE);
            }

            UpdateButtonColors();
            InvalidateRect(hwnd, nullptr, FALSE);
            SetFocus(hwnd);

            return 0;
        }

        case ID_RESET_VIEW:
        {
            // ズーム率だけ100%に戻す
            // 画面位置は変更しない
            g_zoom = INITIAL_ZOOM;

            UpdateView();
            InvalidateRect(hwnd, nullptr, FALSE);

            return 0;
        }

        case ID_BLOCK_LIST:
            return 0;
        }

        break;
    }

    // マウスホイールによるズーム
    case WM_MOUSEWHEEL:
    {
        POINT mouse =
        {
            GET_X_LPARAM(lp),
            GET_Y_LPARAM(lp)
        };

        ScreenToClient(hwnd, &mouse);

        if (
            mouse.x < CANVAS_LEFT ||
            mouse.y < CANVAS_TOP
            )
        {
            return 0;
        }

        float oldZoom = g_zoom;

        int delta = GET_WHEEL_DELTA_WPARAM(wp);

        float factor = delta > 0
            ? 1.15f
            : 1.0f / 1.15f;

        g_zoom *= factor;

        g_zoom = (std::max)(
            MIN_ZOOM,
            (std::min)(g_zoom, MAX_ZOOM)
            );

        if (oldZoom != g_zoom)
        {
            // マウス位置をズームの中心にする
            float worldX =
                (mouse.x - g_viewX) / oldZoom;

            float worldY =
                (mouse.y - g_viewY) / oldZoom;

            g_viewX = mouse.x - worldX * g_zoom;
            g_viewY = mouse.y - worldY * g_zoom;

            UpdateView();
            InvalidateRect(hwnd, nullptr, FALSE);
        }

        return 0;
    }

    // 右クリックで画面移動開始
    case WM_RBUTTONDOWN:
    {
        if (x >= CANVAS_LEFT && y >= CANVAS_TOP)
        {
            // ブロック操作を終了
            g_manager.OnMouseUp(hwnd);

            g_panning = true;
            g_lastPanPoint = { x, y };

            SetCapture(hwnd);
            SetCursor(LoadCursorW(nullptr, IDC_SIZEALL));
        }

        return 0;
    }

    // 右ボタンを離して画面移動終了
    case WM_RBUTTONUP:
    {
        g_panning = false;

        if (GetCapture() == hwnd)
            ReleaseCapture();

        SetCursor(LoadCursorW(nullptr, IDC_ARROW));

        return 0;
    }

    // 左クリック・左ドラッグ
    case WM_LBUTTONDOWN:
    {
        if (x >= CANVAS_LEFT && y >= CANVAS_TOP)
        {
            if (g_deleteMode)
            {
                // 削除モード
                POINT world =
                    g_manager.ScreenToWorld(x, y);

                g_manager.DeleteBlockAt(
                    world.x,
                    world.y
                );

                InvalidateRect(hwnd, nullptr, FALSE);
            }
            else
            {
                // 通常のブロック操作
                g_manager.OnMouseDown(hwnd, x, y);
            }
        }
        else if (
            x >= 10 && x <= 140 &&
            y >= 10 && y <= 48
            )
        {
            // Java出力
            if (ExportJava(g_manager.GetBlocks()))
            {
                MessageBoxW(
                    hwnd,
                    L"Java output completed.",
                    L"Java output",
                    MB_OK | MB_ICONINFORMATION
                );
            }
            else
            {
                MessageBoxW(
                    hwnd,
                    L"Failed to create the Java file.",
                    L"Error",
                    MB_OK | MB_ICONERROR
                );
            }
        }

        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }

    // マウス移動・右ドラッグによる画面移動
    case WM_MOUSEMOVE:
    {
        if (g_panning && (wp & MK_RBUTTON))
        {
            g_viewX += static_cast<float>(
                x - g_lastPanPoint.x
                );

            g_viewY += static_cast<float>(
                y - g_lastPanPoint.y
                );

            g_lastPanPoint = { x, y };

            UpdateView();
            InvalidateRect(hwnd, nullptr, FALSE);

            return 0;
        }

        if (g_panning)
        {
            g_panning = false;

            if (GetCapture() == hwnd)
                ReleaseCapture();
        }

        // 右ドラッグ中以外はブロック操作
        g_manager.OnMouseMove(x, y);

        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }

    // 左ボタンを離す
    case WM_LBUTTONUP:
    {
        g_manager.OnMouseUp(hwnd);
        InvalidateRect(hwnd, nullptr, FALSE);

        return 0;
    }

    // マウスキャプチャが外れた場合
    case WM_CAPTURECHANGED:
    {
        g_panning = false;
        break;
    }

    // ダブルバッファ描画
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        RECT client;
        GetClientRect(hwnd, &client);

        int width = client.right;
        int height = client.bottom;

        if (width > 0 && height > 0)
        {
            HDC memDC = CreateCompatibleDC(hdc);

            HBITMAP bitmap =
                CreateCompatibleBitmap(hdc, width, height);

            HGDIOBJ oldBitmap =
                SelectObject(memDC, bitmap);

            FillRect(
                memDC,
                &client,
                (HBRUSH)GetStockObject(WHITE_BRUSH)
            );

            // Java出力ボタン
            RECT javaButton = { 10, 10, 140, 48 };

            HBRUSH javaBrush =
                CreateSolidBrush(RGB(45, 45, 55));

            FillRect(memDC, &javaButton, javaBrush);
            DeleteObject(javaBrush);

            SetBkMode(memDC, TRANSPARENT);
            SetTextColor(memDC, RGB(255, 255, 255));

            DrawTextW(
                memDC,
                L"Java output",
                -1,
                &javaButton,
                DT_CENTER | DT_VCENTER | DT_SINGLELINE
            );

            // 作業エリア境界線
            HPEN pen = CreatePen(
                PS_SOLID,
                1,
                RGB(220, 220, 220)
            );

            HGDIOBJ oldPen = SelectObject(memDC, pen);

            MoveToEx(memDC, CANVAS_LEFT, 0, nullptr);
            LineTo(memDC, CANVAS_LEFT, height);

            SelectObject(memDC, oldPen);
            DeleteObject(pen);

            // ブロック描画
            g_manager.DrawAll(memDC);

            // ズーム表示
            wchar_t zoomText[64];

            swprintf_s(
                zoomText,
                L"Zoom: %.0f%%",
                g_zoom * 100.0f
            );

            RECT zoomRect = { 10, 100, 520, 48 };

            SetTextColor(memDC, RGB(70, 70, 70));

            DrawTextW(
                memDC,
                zoomText,
                -1,
                &zoomRect,
                DT_LEFT | DT_VCENTER | DT_SINGLELINE
            );

            // 一括転送
            BitBlt(
                hdc,
                0,
                0,
                width,
                height,
                memDC,
                0,
                0,
                SRCCOPY
            );

            SelectObject(memDC, oldBitmap);
            DeleteObject(bitmap);
            DeleteDC(memDC);
        }

        EndPaint(hwnd, &ps);
        return 0;
    }


    case WM_DESTROY:
    {
        if (g_blockList && g_oldListProc)
        {
            SetWindowLongPtrW(
                g_blockList,
                GWLP_WNDPROC,
                reinterpret_cast<LONG_PTR>(g_oldListProc)
            );
        }

        PostQuitMessage(0);
        return 0;
    }
    }

    return DefWindowProcW(hwnd, msg, wp, lp);
}


// アプリケーション開始
int WINAPI wWinMain(
    HINSTANCE instance,
    HINSTANCE,
    PWSTR,
    int show)
{
    const wchar_t CLASS_NAME[] = L"MyBlockEditor";

    WNDCLASSW wc = {};

    wc.lpfnWndProc = WndProc;
    wc.hInstance = instance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;

    if (!RegisterClassW(&wc))
        return 0;

    HWND hwnd = CreateWindowExW(
        0,
        CLASS_NAME,
        L"自作ブロックエディタ",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        1000,
        700,
        nullptr,
        nullptr,
        instance,
        nullptr
    );

    if (!hwnd)
        return 0;

    ShowWindow(hwnd, show);
    UpdateWindow(hwnd);

    MSG msg = {};

    while (GetMessageW(&msg, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return static_cast<int>(msg.wParam);
}