#include <windows.h>
#include <windowsx.h>
#include <cstdlib>
#include <cmath>
#include <string>
#include <vector>
#include <algorithm>
#include <cwchar>

#include <commdlg.h>

#include "BlockManager.h"
#include "JavaGenerator.h"
#include "BlockFileManager.h"

#pragma comment(lib, "Comdlg32.lib")

// ============================================================
// ブロック管理
// ============================================================

BlockManager g_manager;

// ============================================================
// ウィンドウハンドル
// ============================================================

HWND g_blockList = nullptr;
HWND g_addButton = nullptr;
HWND g_deleteButton = nullptr;
HWND g_resetViewButton = nullptr;
HWND g_mainWindow = nullptr;

WNDPROC g_oldListProc = nullptr;

// ============================================================
// 状態管理
// ============================================================

bool g_deleteMode = false;
bool g_showBlockList = false;
bool g_draggingFromList = false;
bool g_panning = false;

BlockType g_dragBlockType = BlockType::Move;

POINT g_dragStart = { 0, 0 };
POINT g_lastPanPoint = { 0, 0 };

POINT g_listDragPoint = { 0, 0 };

// ============================================================
// ズーム・表示位置
// ============================================================

float g_zoom = 1.0f;
float g_viewX = 350.0f;
float g_viewY = 0.0f;

constexpr float MIN_ZOOM = 0.25f;
constexpr float MAX_ZOOM = 3.0f;

constexpr float INITIAL_ZOOM = 1.0f;
constexpr float INITIAL_VIEW_X = 350.0f;
constexpr float INITIAL_VIEW_Y = 0.0f;

// ============================================================
// 画面設定
// ============================================================

constexpr int CANVAS_LEFT = 420;
constexpr int CANVAS_TOP = 50;

// ============================================================
// 既存ボタンID
// ============================================================

constexpr int ID_JAVA = 1001;
constexpr int ID_ADD = 1002;
constexpr int ID_DELETE = 1003;
constexpr int ID_BLOCK_LIST = 1004;
constexpr int ID_RESET_VIEW = 1005;
constexpr int ID_SAVE = 1006;
constexpr int ID_LOAD = 1007;

// キーボードショートカット用 ID
constexpr int ID_SHORTCUT_SELECT_ALL = 1010;
constexpr int ID_SHORTCUT_SAVE = 1011;
constexpr int ID_SHORTCUT_LOAD = 1012;
constexpr int ID_SHORTCUT_UNDO = 1013;
constexpr int ID_SHORTCUT_REDO = 1014;
constexpr int ID_SHORTCUT_COPY = 1015;
constexpr int ID_SHORTCUT_PASTE = 1016;
constexpr int ID_SHORTCUT_DELETE = 1017;

// アクセラレータテーブル
HACCEL g_hAccel = nullptr;

// ============================================================
// カテゴリボタンID
// ============================================================

constexpr int ID_CATEGORY_BASE = 2000;

// ============================================================
// カテゴリの種類
// ============================================================

enum class BlockCategory
{
    Move,
    Action,
    Control,
    Variables,
    Other,
    Count
};

constexpr int CATEGORY_COUNT =
static_cast<int>(BlockCategory::Count);

// ============================================================
// カテゴリ設定
// ============================================================

const wchar_t* const CATEGORY_NAMES[CATEGORY_COUNT] =
{
    L"移動",
    L"アクション",
    L"制御",
    L"変数",
    L"その他"
};

const COLORREF CATEGORY_COLORS[CATEGORY_COUNT] =
{
    RGB(65, 150, 230),   // 移動
    RGB(235, 165, 45),   // アクション
    RGB(225, 185, 45),   // 制御
    RGB(220, 90, 150),   // 変数
    RGB(120, 120, 130)   // その他
};

// カテゴリボタン
HWND g_categoryButtons[CATEGORY_COUNT] = {};

// 現在選択中のカテゴリ
BlockCategory g_selectedCategory = BlockCategory::Move;

// 現在のカテゴリに表示するブロック
std::vector<BlockType> g_visibleBlockTypes;

// ボタンの色
const COLORREF COLOR_NORMAL = RGB(65, 65, 75);
const COLORREF COLOR_ADD_ON = RGB(40, 120, 220);
const COLORREF COLOR_DELETE_ON = RGB(220, 65, 65);

// ============================================================
// 座標変換を更新
// ============================================================

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

// ============================================================
// カテゴリ別のブロック一覧
// ============================================================

std::vector<BlockType> GetBlockTypes(
    BlockCategory category)
{
    switch (category)
    {
    case BlockCategory::Move:
        return
        {
            BlockType::Move,
            BlockType::Turn,
            BlockType::TurnLeft,
            BlockType::MoveRight,
            BlockType::MoveLeft
        };

    case BlockCategory::Action:
        return
        {
            BlockType::Fire,
            BlockType::Bark
        };

    case BlockCategory::Control:
        return
        {
            BlockType::Repeat,
            BlockType::EndRepeat,
            BlockType::Wait,
            BlockType::If,
            BlockType::Else,
            BlockType::EndIf
        };

    case BlockCategory::Variables:
        return
        {
            BlockType::CreateVariable,
            BlockType::SetVariable,
            BlockType::ChangeVariable,
            BlockType::AddVariable,
            BlockType::SubtractVariable,
            BlockType::SetBoolean
        };

    case BlockCategory::Other:
    default:
        return {};
    }
}

// ============================================================
// ブロック一覧の更新
// ============================================================

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

    g_visibleBlockTypes =
        GetBlockTypes(g_selectedCategory);

    for (BlockType type : g_visibleBlockTypes)
    {
        std::wstring name = GetBlockName(type);

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

// ============================================================
// カテゴリを切り替える
// ============================================================

void SelectCategory(BlockCategory category)
{
    if (category < BlockCategory::Move ||
        category >= BlockCategory::Count)
    {
        return;
    }

    g_selectedCategory = category;

    // 選択したカテゴリのブロックを一覧に表示
    InitializeBlockList(g_blockList);

    // 一覧を表示する
    g_showBlockList = true;

    if (g_blockList)
        ShowWindow(g_blockList, SW_SHOW);

    // 削除モードを解除
    if (g_deleteMode)
    {
        g_deleteMode = false;
        g_manager.SetDeleteMode(false);
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
        p.x >= CANVAS_LEFT &&
        p.y >= CANVAS_TOP &&
        p.x < client.right &&
        p.y < client.bottom;
}

// ============================================================
// ブロック一覧からドラッグ＆ドロップ
// ============================================================


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
    case WM_LBUTTONDOWN:
    {
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

            g_draggingFromList = true;

            // マウス位置をメインウィンドウ座標へ変換
            POINT p = {
                GET_X_LPARAM(lp),
                GET_Y_LPARAM(lp)
            };

            ClientToScreen(hwnd, &p);
            ScreenToClient(g_mainWindow, &p);

            g_listDragPoint = p;

            // メインウィンドウでドラッグ操作を受け取る
            SetCapture(g_mainWindow);

            InvalidateEditor();
        }

        break;
    }
    }

    return result;
}

// ============================================================
// カテゴリボタンを作成
// ============================================================

void CreateCategoryButtons(HWND hwnd)
{
    constexpr int BUTTON_X = 10;
    constexpr int BUTTON_Y = 60;
    constexpr int BUTTON_WIDTH = 125;
    constexpr int BUTTON_HEIGHT = 42;
    constexpr int BUTTON_GAP = 8;

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
    }
}

// ============================================================
// カテゴリボタンの描画
// ============================================================

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

// ============================================================
// プロジェクト保存
// ============================================================

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

// ============================================================
// プロジェクト読み込み
// ============================================================

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

    InvalidateEditor();
    return true;
}


LRESULT CALLBACK WndProc(
    HWND hwnd,
    UINT msg,
    WPARAM wParam,
    LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
    {
        g_mainWindow = hwnd;

        g_manager.Initialize();
        UpdateView();

        // 左側のカテゴリボタン
        CreateCategoryButtons(hwnd);

        // ブロック一覧
        g_blockList = CreateWindowW(
            L"LISTBOX",
            nullptr,
            WS_CHILD | WS_VISIBLE |
            WS_BORDER | WS_VSCROLL |
            LBS_NOTIFY | LBS_NOINTEGRALHEIGHT,
            150, 60, 180, 190,
            hwnd,
            reinterpret_cast<HMENU>(
                static_cast<INT_PTR>(ID_BLOCK_LIST)),
            GetModuleHandleW(nullptr),
            nullptr
        );

        InitializeBlockList(g_blockList);

        // リストからキャンバスへドラッグ
        g_oldListProc = reinterpret_cast<WNDPROC>(
            SetWindowLongPtrW(
                g_blockList,
                GWLP_WNDPROC,
                reinterpret_cast<LONG_PTR>(BlockListProc)
            )
            );

        // ブロック追加
        g_addButton = CreateWindowW(
            L"BUTTON",
            L"追加",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            150, 270, 85, 35,
            hwnd,
            reinterpret_cast<HMENU>(
                static_cast<INT_PTR>(ID_ADD)),
            GetModuleHandleW(nullptr),
            nullptr
        );

        // 削除モード
        g_deleteButton = CreateWindowW(
            L"BUTTON",
            L"削除モード",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            245, 270, 100, 35,
            hwnd,
            reinterpret_cast<HMENU>(
                static_cast<INT_PTR>(ID_DELETE)),
            GetModuleHandleW(nullptr),
            nullptr
        );

        // 表示位置リセット
        g_resetViewButton = CreateWindowW(
            L"BUTTON",
            L"表示リセット",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            150, 315, 195, 35,
            hwnd,
            reinterpret_cast<HMENU>(
                static_cast<INT_PTR>(ID_RESET_VIEW)),
            GetModuleHandleW(nullptr),
            nullptr
        );

        // 保存
        CreateWindowW(
            L"BUTTON",
            L"保存",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            150, 360, 90, 35,
            hwnd,
            reinterpret_cast<HMENU>(
                static_cast<INT_PTR>(ID_SAVE)),
            GetModuleHandleW(nullptr),
            nullptr
        );

        // 読み込み
        CreateWindowW(
            L"BUTTON",
            L"読み込み",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            250, 360, 95, 35,
            hwnd,
            reinterpret_cast<HMENU>(
                static_cast<INT_PTR>(ID_LOAD)),
            GetModuleHandleW(nullptr),
            nullptr
        );

        UpdateButtonColors();

        return 0;
    }

    // ========================================================
    // ボタン描画
    // ========================================================

    case WM_DRAWITEM:
    {
        const DRAWITEMSTRUCT* dis =
            reinterpret_cast<const DRAWITEMSTRUCT*>(lParam);

        if (!dis)
            return TRUE;

        int id = static_cast<int>(dis->CtlID);

        if (id >= ID_CATEGORY_BASE &&
            id < ID_CATEGORY_BASE + CATEGORY_COUNT)
        {
            DrawCategoryButton(
                dis,
                id - ID_CATEGORY_BASE
            );
            return TRUE;
        }

        if (id == ID_ADD)
        {
            DrawCustomButton(
                dis,
                RGB(45, 145, 75),
                L"追加"
            );
            return TRUE;
        }

        if (id == ID_DELETE)
        {
            DrawCustomButton(
                dis,
                g_deleteMode
                ? COLOR_DELETE_ON
                : COLOR_NORMAL,
                g_deleteMode
                ? L"削除中"
                : L"削除モード"
            );
            return TRUE;
        }

        if (id == ID_RESET_VIEW)
        {
            DrawCustomButton(
                dis,
                COLOR_NORMAL,
                L"表示リセット"
            );
            return TRUE;
        }

        if (id == ID_SAVE)
        {
            DrawCustomButton(
                dis,
                COLOR_NORMAL,
                L"保存"
            );
            return TRUE;
        }

        if (id == ID_LOAD)
        {
            DrawCustomButton(
                dis,
                COLOR_NORMAL,
                L"読み込み"
            );
            return TRUE;
        }

        return TRUE;
    }

    // ========================================================
    // キーボードショートカット
    // ========================================================

    case WM_KEYDOWN:
    {
        // Deleteキー
        if (wParam == VK_DELETE)
        {
            if (g_manager.DeleteSelectedBlock())
                InvalidateEditor();

            return 0;
        }

        bool ctrlPressed =
            (GetKeyState(VK_CONTROL) & 0x8000) != 0;

        if (ctrlPressed)
        {
            switch (wParam)
            {
            case 'A':
                g_manager.SelectAllBlocks();
                InvalidateEditor();
                return 0;

            case 'S':
                SaveProject(hwnd);
                return 0;

            case 'O':
                LoadProject(hwnd);
                InvalidateEditor();
                return 0;

            case 'Z':
                if (g_manager.Undo())
                    InvalidateEditor();
                return 0;

            case 'Y':
                if (g_manager.Redo())
                    InvalidateEditor();
                return 0;

            case 'C':
                g_manager.CopySelectedBlock();
                return 0;

            case 'V':
            {
                const auto& blocks = g_manager.GetBlocks();
                int index = g_manager.GetSelectedBlockIndex();

                if (index >= 0 &&
                    index < static_cast<int>(blocks.size()))
                {
                    int x = blocks[index].x + 30;
                    int y = blocks[index].y + 30;

                    if (g_manager.PasteCopiedBlock(x, y))
                        InvalidateEditor();
                }

                return 0;
            }
            }
        }

        break;
    }

    // ========================================================
    // ボタンクリック
    // ========================================================

    case WM_COMMAND:
    {
        int id = LOWORD(wParam);
        int notification = HIWORD(wParam);

        // カテゴリ切り替え
        if (id >= ID_CATEGORY_BASE &&
            id < ID_CATEGORY_BASE + CATEGORY_COUNT)
        {
            if (notification == BN_CLICKED)
            {
                SelectCategory(
                    static_cast<BlockCategory>(
                        id - ID_CATEGORY_BASE
                        )
                );
            }

            return 0;
        }

        switch (id)
        {
            // ====================================================
// キーボードショートカット
// ====================================================

        case ID_SHORTCUT_SELECT_ALL:
            g_manager.SelectAllBlocks();
            InvalidateEditor();
            return 0;

        case ID_SHORTCUT_SAVE:
            SaveProject(hwnd);
            return 0;

        case ID_SHORTCUT_LOAD:
            LoadProject(hwnd);
            InvalidateEditor();
            return 0;

        case ID_SHORTCUT_UNDO:
            if (g_manager.Undo())
                InvalidateEditor();
            return 0;

        case ID_SHORTCUT_REDO:
            if (g_manager.Redo())
                InvalidateEditor();
            return 0;

        case ID_SHORTCUT_COPY:
            g_manager.CopySelectedBlock();
            return 0;

        case ID_SHORTCUT_PASTE:
        {
            const auto& blocks = g_manager.GetBlocks();
            int index = g_manager.GetSelectedBlockIndex();

            if (index >= 0 &&
                index < static_cast<int>(blocks.size()))
            {
                int x = blocks[index].x + 30;
                int y = blocks[index].y + 30;

                if (g_manager.PasteCopiedBlock(x, y))
                    InvalidateEditor();
            }

            return 0;
        }

        case ID_SHORTCUT_DELETE:
            if (g_manager.DeleteSelectedBlock())
                InvalidateEditor();
            return 0;
        case ID_ADD:
            if (notification == BN_CLICKED)
            {
                POINT center = {
                    CANVAS_LEFT + 100,
                    CANVAS_TOP + 100
                };

                POINT world =
                    g_manager.ScreenToWorld(
                        center.x,
                        center.y
                    );

                LRESULT selectedIndex = SendMessageW(
                    g_blockList,
                    LB_GETCURSEL,
                    0,
                    0
                );

                int index = selectedIndex == LB_ERR
                    ? 0
                    : static_cast<int>(selectedIndex);

                if (!g_visibleBlockTypes.empty())
                {
                    if (index >= 0 &&
                        index < static_cast<int>(
                            g_visibleBlockTypes.size()))
                    {
                        g_manager.AddBlock(
                            GetBlockTypeFromIndex(index),
                            world.x,
                            world.y
                        );

                        InvalidateEditor();
                    }
                }
            }
            return 0;

        case ID_DELETE:
            if (notification == BN_CLICKED)
            {
                g_deleteMode = !g_deleteMode;

                g_manager.SetDeleteMode(g_deleteMode);

                UpdateButtonColors();
                InvalidateEditor();
            }
            return 0;

        case ID_RESET_VIEW:
            if (notification == BN_CLICKED)
            {
                g_zoom = INITIAL_ZOOM;
                g_viewX = INITIAL_VIEW_X;
                g_viewY = INITIAL_VIEW_Y;

                UpdateView();
                InvalidateEditor();
            }
            return 0;

        case ID_SAVE:
            if (notification == BN_CLICKED)
                SaveProject(hwnd);
            return 0;

        case ID_LOAD:
            if (notification == BN_CLICKED)
            {
                LoadProject(hwnd);
                InvalidateEditor();
            }
            return 0;
        }

        break;
    }

    // ========================================================
    // マウスホイール：ズーム
    // ========================================================

    case WM_MOUSEWHEEL:
    {
        POINT mouse = {
            GET_X_LPARAM(lParam),
            GET_Y_LPARAM(lParam)
        };

        ScreenToClient(hwnd, &mouse);

        float oldZoom = g_zoom;

        float factor =
            GET_WHEEL_DELTA_WPARAM(wParam) > 0
            ? 1.1f
            : 1.0f / 1.1f;

        float newZoom = oldZoom * factor;

        newZoom = (std::max)(
            MIN_ZOOM,
            (std::min)(newZoom, MAX_ZOOM)
            );

        float worldX = (mouse.x - g_viewX) / oldZoom;
        float worldY = (mouse.y - g_viewY) / oldZoom;

        g_zoom = newZoom;
        g_viewX = mouse.x - worldX * g_zoom;
        g_viewY = mouse.y - worldY * g_zoom;

        UpdateView();
        InvalidateEditor();

        return 0;
    }

    // ========================================================
    // 右クリック：キャンバス移動
    // ========================================================

    case WM_RBUTTONDOWN:
    {
        POINT p = {
            GET_X_LPARAM(lParam),
            GET_Y_LPARAM(lParam)
        };

        if (IsCanvasPoint(p))
        {
            g_panning = true;
            g_lastPanPoint = p;

            SetCapture(hwnd);
        }

        return 0;
    }

    case WM_RBUTTONUP:
    {
        if (g_panning)
        {
            g_panning = false;

            if (GetCapture() == hwnd)
                ReleaseCapture();
        }

        return 0;
    }

    // ========================================================
    // マウス移動
    // ========================================================

    case WM_MOUSEMOVE:
    {
        POINT p = {
            GET_X_LPARAM(lParam),
            GET_Y_LPARAM(lParam)
        };

        // リストからブロックをドラッグ中
        if (g_draggingFromList)
        {
            g_listDragPoint = p;

            InvalidateEditor();

            return 0;
        }

        // キャンバス移動
        if (g_panning)
        {
            g_viewX += p.x - g_lastPanPoint.x;
            g_viewY += p.y - g_lastPanPoint.y;

            g_lastPanPoint = p;

            UpdateView();
            InvalidateEditor();

            return 0;
        }

        // 左ボタンを押している間
        if (wParam & MK_LBUTTON)
        {
            if (g_manager.IsSelectingRange())
            {
                // 範囲選択はワールド座標で更新
                POINT world = g_manager.ScreenToWorld(
                    p.x,
                    p.y
                );

                g_manager.UpdateRangeSelection(
                    world.x,
                    world.y
                );
            }
            else
            {
                // 通常のブロック移動
                g_manager.OnMouseMove(p.x, p.y);
            }

            InvalidateEditor();
        }

        return 0;
    }

    // ========================================================
    // 左クリック：ブロック操作・範囲選択
    // ========================================================


    case WM_LBUTTONDOWN:
    {
        POINT p = {
            GET_X_LPARAM(lParam),
            GET_Y_LPARAM(lParam)
        };

        // Java出力ボタン
        if (p.x >= 10 && p.x <= 140 &&
            p.y >= 10 && p.y <= 48)
        {
            ExportJava(g_manager.GetBlocks());
            return 0;
        }

        if (!IsCanvasPoint(p))
            return 0;

        POINT world = g_manager.ScreenToWorld(p.x, p.y);

        const auto& blocks = g_manager.GetBlocks();
        bool hitBlock = false;

        // 手前のブロックから判定する
        for (int i = static_cast<int>(blocks.size()) - 1;
            i >= 0; --i)
        {
            const Block& block = blocks[i];

            if (world.x >= block.x &&
                world.x < block.x + block.w &&
                world.y >= block.y &&
                world.y < block.y + block.h)
            {
                hitBlock = true;
                break;
            }
        }

        // 削除モードでは、クリックしたブロックを削除
        if (g_deleteMode)
        {
            if (hitBlock)
            {
                g_manager.DeleteBlockAt(world.x, world.y);
                InvalidateEditor();
            }

            return 0;
        }

        // 空白からドラッグしたら範囲選択
        if (!hitBlock)
        {
            g_manager.StartRangeSelection(world.x, world.y);

            SetCapture(hwnd);
            InvalidateEditor();

            return 0;
        }

        // ブロックをクリックしたら通常の選択・移動
        g_manager.OnMouseDown(hwnd, p.x, p.y);

        SetCapture(hwnd);
        InvalidateEditor();

        return 0;
    }


    // ========================================================
    // 左ボタンを離す：ドラッグ・範囲選択終了
    // ========================================================

    case WM_LBUTTONUP:
    {
        // リストからドラッグしたブロックをドロップ
        if (g_draggingFromList)
        {
            POINT p = {
                GET_X_LPARAM(lParam),
                GET_Y_LPARAM(lParam)
            };

            // ドラッグ状態を先に解除
            g_draggingFromList = false;

            if (IsCanvasPoint(p))
            {
                // キャンバス座標をブロックのワールド座標に変換
                POINT world = g_manager.ScreenToWorld(
                    p.x,
                    p.y
                );

                // ドロップ位置にブロックを追加
                g_manager.AddBlock(
                    g_dragBlockType,
                    world.x,
                    world.y
                );
            }

            if (GetCapture() == hwnd)
                ReleaseCapture();

            InvalidateEditor();

            return 0;
        }

        if (g_manager.IsSelectingRange())
        {
            POINT p = {
                GET_X_LPARAM(lParam),
                GET_Y_LPARAM(lParam)
            };

            POINT world = g_manager.ScreenToWorld(
                p.x,
                p.y
            );

            // 最終位置を反映
            g_manager.UpdateRangeSelection(
                world.x,
                world.y
            );

            g_manager.EndRangeSelection();
        }
        else
        {
            g_manager.OnMouseUp(hwnd);
        }

        if (GetCapture() == hwnd)
            ReleaseCapture();

        InvalidateEditor();

        return 0;
    }

    // ========================================================
    // マウス捕捉の変更
    // ========================================================

    case WM_CAPTURECHANGED:
    {
        g_panning = false;
        g_draggingFromList = false;

        return 0;
    }

    // ========================================================
    // 画面描画
    // ========================================================

    case WM_PAINT:
    {
        PAINTSTRUCT ps{};
        HDC hdc = BeginPaint(hwnd, &ps);

        RECT rc{};
        GetClientRect(hwnd, &rc);

        HDC memDC = CreateCompatibleDC(hdc);

        HBITMAP memBitmap = CreateCompatibleBitmap(
            hdc,
            rc.right,
            rc.bottom
        );

        HGDIOBJ oldBitmap = SelectObject(
            memDC,
            memBitmap
        );

        // 背景
        HBRUSH background =
            CreateSolidBrush(RGB(35, 35, 42));

        FillRect(memDC, &rc, background);
        DeleteObject(background);

        // Java出力ボタン
        RECT javaRect = { 10, 10, 140, 48 };

        HBRUSH javaBrush =
            CreateSolidBrush(RGB(45, 110, 190));

        FillRect(memDC, &javaRect, javaBrush);
        DeleteObject(javaBrush);

        SetBkMode(memDC, TRANSPARENT);
        SetTextColor(memDC, RGB(255, 255, 255));

        DrawTextW(
            memDC,
            L"Java出力",
            -1,
            &javaRect,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE
        );

        // キャンバス
        RECT canvasRect = {
            CANVAS_LEFT,
            CANVAS_TOP,
            rc.right - 20,
            rc.bottom - 20
        };

        HBRUSH canvasBrush =
            CreateSolidBrush(RGB(245, 245, 245));

        FillRect(memDC, &canvasRect, canvasBrush);
        DeleteObject(canvasBrush);

        HBRUSH canvasBorder =
            CreateSolidBrush(RGB(100, 100, 100));

        FrameRect(memDC, &canvasRect, canvasBorder);
        DeleteObject(canvasBorder);

        // ブロックと選択枠
        g_manager.DrawAll(memDC);

        // ドラッグ中のブロック名を表示
        if (g_draggingFromList)
        {
            std::wstring name = GetBlockName(g_dragBlockType);

            RECT dragRect = {
                g_listDragPoint.x + 12,
                g_listDragPoint.y + 12,
                g_listDragPoint.x + 180,
                g_listDragPoint.y + 42
            };

            HBRUSH dragBrush =
                CreateSolidBrush(RGB(65, 120, 200));

            FillRect(memDC, &dragRect, dragBrush);
            DeleteObject(dragBrush);

            SetBkMode(memDC, TRANSPARENT);
            SetTextColor(memDC, RGB(255, 255, 255));

            DrawTextW(
                memDC,
                name.c_str(),
                -1,
                &dragRect,
                DT_CENTER | DT_VCENTER | DT_SINGLELINE
            );
        }

        // ズーム表示
        wchar_t zoomText[64]{};

        swprintf_s(
            zoomText,
            L"Zoom: %.0f%%",
            g_zoom * 100.0f
        );

        SetTextColor(memDC, RGB(255, 255, 255));

        TextOutW(
            memDC,
            CANVAS_LEFT,
            15,
            zoomText,
            static_cast<int>(wcslen(zoomText))
        );

        // バッファを画面へ転送
        BitBlt(
            hdc,
            0,
            0,
            rc.right,
            rc.bottom,
            memDC,
            0,
            0,
            SRCCOPY
        );

        SelectObject(memDC, oldBitmap);
        DeleteObject(memBitmap);
        DeleteDC(memDC);

        EndPaint(hwnd, &ps);

        return 0;
    }

    // ========================================================
    // 終了処理
    // ========================================================

    case WM_DESTROY:
    {
        if (g_blockList && g_oldListProc)
        {
            SetWindowLongPtrW(
                g_blockList,
                GWLP_WNDPROC,
                reinterpret_cast<LONG_PTR>(
                    g_oldListProc
                    )
            );
        }

        PostQuitMessage(0);
        return 0;
    }
    }

    return DefWindowProcW(
        hwnd,
        msg,
        wParam,
        lParam
    );
}



int WINAPI WinMain(
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPSTR lpCmdLine,
    int nCmdShow)
{
    (void)hPrevInstance;
    (void)lpCmdLine;

    // ウィンドウクラスの登録
    const wchar_t CLASS_NAME[] = L"MyBlockEditor";

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground =
        reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = CLASS_NAME;

    if (!RegisterClassExW(&wc))
    {
        MessageBoxW(
            nullptr,
            L"ウィンドウクラスの登録に失敗しました。",
            L"エラー",
            MB_OK | MB_ICONERROR
        );

        return 1;
    }

    // メインウィンドウの作成
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
        hInstance,
        nullptr
    );

    if (!hwnd)
    {
        MessageBoxW(
            nullptr,
            L"ウィンドウの作成に失敗しました。",
            L"エラー",
            MB_OK | MB_ICONERROR
        );

        return 1;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);
    ACCEL accel[] =
    {
        { FVIRTKEY | FCONTROL, 'A', ID_SHORTCUT_SELECT_ALL },
        { FVIRTKEY | FCONTROL, 'S', ID_SHORTCUT_SAVE },
        { FVIRTKEY | FCONTROL, 'O', ID_SHORTCUT_LOAD },
        { FVIRTKEY | FCONTROL, 'Z', ID_SHORTCUT_UNDO },
        { FVIRTKEY | FCONTROL, 'Y', ID_SHORTCUT_REDO },
        { FVIRTKEY | FCONTROL, 'C', ID_SHORTCUT_COPY },
        { FVIRTKEY | FCONTROL, 'V', ID_SHORTCUT_PASTE },
        { FVIRTKEY, VK_DELETE, ID_SHORTCUT_DELETE }
    };

    g_hAccel = CreateAcceleratorTableW(
        accel,
        static_cast<int>(sizeof(accel) / sizeof(accel[0]))
    );

    if (!g_hAccel)
    {
        MessageBoxW(
            hwnd,
            L"ショートカットの登録に失敗しました。",
            L"エラー",
            MB_OK | MB_ICONERROR
        );
    }

    // メッセージループ
    MSG msg{};

    while (GetMessageW(&msg, nullptr, 0, 0) > 0)
    {
        if (g_hAccel &&
            TranslateAcceleratorW(hwnd, g_hAccel, &msg))
        {
            continue;
        }

        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return static_cast<int>(msg.wParam);
}
