#include <shlobj.h>
#include <filesystem>
#include <windows.h>
#include <windowsx.h>
#include <cstdlib>
#include <cmath>
#include <string>
#include <vector>
#include <algorithm>
#include <cwchar>
#include <utility>

#include <commdlg.h>
#include <gdiplus.h>
#include <map>

#include "BlockManager.h"
#include "JavaGenerator.h"
#include "BlockFileManager.h"
#include "BlockColors.h"
#include "UserFunction.h"
#include "UserFunctionStorage.h"

#pragma comment(lib, "Comdlg32.lib")
#pragma comment(lib, "gdiplus.lib")

using namespace Gdiplus;

// ============================================================
// ブロック管理
// ============================================================

BlockManager g_manager;
BlockManager g_functionManager;
bool g_isEditingFunction = false;
std::wstring g_currentFunctionName;
std::vector<UserDefinedFunction> g_userFunctions;

// ============================================================
// ウィンドウハンドル
// ============================================================

HWND g_blockList = nullptr;
HWND g_addButton = nullptr;
HWND g_deleteButton = nullptr;
HWND g_resetViewButton = nullptr;
HWND g_helpButton = nullptr;
HWND g_mainWindow = nullptr;
HWND g_numericEdit = nullptr;
HWND g_undoButton = nullptr;
HWND g_redoButton = nullptr;
HWND g_editRobotName = nullptr;
HWND g_editFolderPath = nullptr;
int g_activeSidebarTab = 0; // 0: メイン, 1: 関数一覧, 2: 関数編集中
HWND g_tabMainBtn = nullptr;
HWND g_tabFuncBtn = nullptr;
HWND g_createdFuncList = nullptr; // 作成した関数を表示するリストボックス
HWND g_functionHelpLabel = nullptr; // 関数一覧・編集の操作説明
int g_numericEditBlockIndex = -1;
int g_numericEditField = 0; // 1=distance, 2=repeatCount, 3=waitSeconds, 4=conditionValue, 5=value
WNDPROC g_oldNumericEditProc = nullptr;
WNDPROC g_oldListProc = nullptr;

ULONG_PTR g_gdiplusToken = 0;

// ツールバーボタン用の GDI+ Bitmap 画像ハンドル
HBITMAP g_hBmpNew = nullptr;
HBITMAP g_hBmpOpen = nullptr;
HBITMAP g_hBmpSave = nullptr;
HBITMAP g_hBmpUndo = nullptr;
HBITMAP g_hBmpRedo = nullptr;
HBITMAP g_hBmpRun = nullptr;

// ============================================================
// 状態管理
// ============================================================

bool g_deleteMode = false;
bool g_showBlockList = false;
bool g_draggingFromList = false;
bool g_panning = false;
bool g_isResizingList = false;

int g_listWidth = 220;

BlockType g_dragBlockType = BlockType::Move;
std::wstring g_dragBlockFunctionName; // ドラッグ中の自作関数名（空なら通常ブロック）

POINT g_dragStart = { 0, 0 };
POINT g_lastPanPoint = { 0, 0 };

POINT g_listDragPoint = { 0, 0 };

// 色変更ブロックのクリック判定用
POINT g_colorClickStart = { 0, 0 };
bool g_colorClickCandidate = false;

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

//constexpr int CANVAS_LEFT = 320;
int g_canvasLeft = 230;
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
constexpr int ID_UNDO = 1018;
constexpr int ID_REDO = 1019;
constexpr int ID_HELP_BUTTON = 1008;
// ツールバー用ID
constexpr int ID_TOOL_NEW = 2101;
constexpr int ID_TOOL_OPEN = 2102;
constexpr int ID_TOOL_SAVE = 2103;
constexpr int ID_TOOL_UNDO = 2104;
constexpr int ID_TOOL_REDO = 2105;
constexpr int ID_TOOL_RUN = 2106;
constexpr int ID_TOOL_JAVA = 2107;
// 画面上部に配置するためのコントロールID
constexpr int ID_EDIT_ROBOT_NAME = 2201;
constexpr int ID_EDIT_FOLDER_PATH = 2202;
constexpr int ID_BTN_BROWSE = 2203;
// 画面上部に配置するためのコントロールID等の追加
constexpr int ID_TAB_MAIN = 2301;
constexpr int ID_TAB_FUNCTION = 2302;
constexpr int ID_CREATED_FUNC_LIST = 2303;

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
    Control,
    Variables,
    Function,
    Event,
    Rotate,
    Color,
    Information,
    System,
    Other,
    Count
};

constexpr int CATEGORY_COUNT =
static_cast<int>(BlockCategory::Count);

// ブロックのインデックスと、移動前の元のX座標を記憶しておくマップ
std::map<size_t, int> g_blockShiftMap;;
std::map<size_t, int> g_originalXMap;

// ============================================================
// カテゴリ設定
// ============================================================

const wchar_t* const CATEGORY_NAMES[CATEGORY_COUNT] =
{
    L"移動",
    L"制御",
    L"変数",
    L"関数",
    L"イベント",
    L"旋回",
    L"色を変更",
    L"情報",
    L"システム",
    L"その他"
};

const COLORREF CATEGORY_COLORS[CATEGORY_COUNT] =
{
    RGB(65, 150, 230),   // 移動
    RGB(235, 165, 45),   // 制御
    RGB(225, 185, 45),   // 変数
    RGB(220, 90, 150),   // 関数
    RGB(50, 190, 190),   // イベント
    RGB(120, 120, 130),  // 旋回
    RGB(220, 90, 90),    // 色を変更
    RGB(100, 170, 110),  // 情報
    RGB(130, 100, 200),  // システム
    RGB(100, 100, 100)   // その他
};

// カテゴリボタン
HWND g_categoryButtons[CATEGORY_COUNT] = {};

// 現在選択中のカテゴリ
BlockCategory g_selectedCategory = BlockCategory::Move;

// ホバー中のカテゴリー番号
int g_hoveredCategory = -1;

// ホバー中のブロック一覧の項目番号
int g_hoveredBlockListItem = -1;

WNDPROC g_oldCategoryProcs[CATEGORY_COUNT] = {};


// 現在のカテゴリに表示するブロック
std::vector<BlockType> g_visibleBlockTypes;
std::vector<std::wstring> g_visibleFunctionNames; // g_visibleBlockTypes と同じ項目数

// ボタンの色
const COLORREF COLOR_NORMAL = RGB(65, 65, 75);
const COLORREF COLOR_ADD_ON = RGB(40, 120, 220);
const COLORREF COLOR_DELETE_ON = RGB(220, 65, 65);

// ============================================================
// 関数編集の共通処理
// ============================================================

BlockManager& ActiveManager()
{
    return g_isEditingFunction ? g_functionManager : g_manager;
}

void InvalidateEditor();
void UpdateButtonColors();
void UpdateView();
void InitializeBlockList(HWND list);

int FindUserFunctionIndex(const std::wstring& name)
{
    for (int i = 0; i < static_cast<int>(g_userFunctions.size()); ++i)
    {
        if (g_userFunctions[static_cast<size_t>(i)].name == name)
            return i;
    }
    return -1;
}

void ReloadUserFunctions()
{
    std::vector<UserDefinedFunction> loaded;
    if (LoadUserFunctions(loaded))
        g_userFunctions = std::move(loaded);
}

std::wstring TrimFunctionName(std::wstring value)
{
    auto isSpace = [](wchar_t ch) { return ch == L' ' || ch == L'\t' || ch == L'\r' || ch == L'\n'; };
    while (!value.empty() && isSpace(value.front())) value.erase(value.begin());
    while (!value.empty() && isSpace(value.back())) value.pop_back();
    return value;
}

struct FunctionNameDialogState
{
    HWND edit = nullptr;
    std::wstring value;
    bool accepted = false;
};

constexpr int ID_FUNCTION_NAME_EDIT = 1;
constexpr int ID_FUNCTION_NAME_OK = 2;
constexpr int ID_FUNCTION_NAME_CANCEL = 3;

LRESULT CALLBACK FunctionNameDialogProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    auto* state = reinterpret_cast<FunctionNameDialogState*>(
        GetWindowLongPtrW(window, GWLP_USERDATA));

    if (message == WM_NCCREATE)
    {
        const auto* createInfo = reinterpret_cast<const CREATESTRUCTW*>(lParam);
        state = static_cast<FunctionNameDialogState*>(createInfo->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
    }

    switch (message)
    {
    case WM_CREATE:
        CreateWindowW(L"STATIC", L"関数の名前を入力してください。", WS_CHILD | WS_VISIBLE,
            18, 16, 340, 24, window, nullptr, GetModuleHandleW(nullptr), nullptr);
        state->edit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"MyFunction",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | WS_TABSTOP,
            18, 44, 340, 28, window,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_FUNCTION_NAME_EDIT)),
            GetModuleHandleW(nullptr), nullptr);
        CreateWindowW(L"BUTTON", L"作成", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
            184, 88, 80, 30, window,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_FUNCTION_NAME_OK)),
            GetModuleHandleW(nullptr), nullptr);
        CreateWindowW(L"BUTTON", L"キャンセル", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            274, 88, 84, 30, window,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_FUNCTION_NAME_CANCEL)),
            GetModuleHandleW(nullptr), nullptr);
        SetFocus(state->edit);
        SendMessageW(state->edit, EM_SETSEL, 0, -1);
        return 0;

    case WM_COMMAND:
        if (LOWORD(wParam) == ID_FUNCTION_NAME_OK)
        {
            wchar_t buffer[256]{};
            GetWindowTextW(state->edit, buffer, 256);
            state->value = TrimFunctionName(buffer);
            state->accepted = true;
            DestroyWindow(window);
            return 0;
        }
        if (LOWORD(wParam) == ID_FUNCTION_NAME_CANCEL)
        {
            DestroyWindow(window);
            return 0;
        }
        break;

    case WM_CLOSE:
        DestroyWindow(window);
        return 0;
    }

    return DefWindowProcW(window, message, wParam, lParam);
}

bool PromptForFunctionName(HWND owner, std::wstring& result)
{
    static bool classRegistered = false;
    const wchar_t* className = L"BlockEditorFunctionNameDialog";

    if (!classRegistered)
    {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = FunctionNameDialogProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        wc.lpszClassName = className;
        if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
            return false;
        classRegistered = true;
    }

    FunctionNameDialogState state;
    HWND dialog = CreateWindowExW(
        WS_EX_DLGMODALFRAME,
        className,
        L"新しい関数を作成",
        WS_CAPTION | WS_SYSMENU | WS_POPUP,
        CW_USEDEFAULT, CW_USEDEFAULT, 390, 165,
        owner, nullptr, GetModuleHandleW(nullptr), &state);

    if (!dialog)
        return false;

    if (owner) EnableWindow(owner, FALSE);
    RECT ownerRect{};
    if (owner && GetWindowRect(owner, &ownerRect))
    {
        SetWindowPos(dialog, HWND_TOP,
            ownerRect.left + ((ownerRect.right - ownerRect.left) - 390) / 2,
            ownerRect.top + ((ownerRect.bottom - ownerRect.top) - 165) / 2,
            390, 165, SWP_SHOWWINDOW);
    }
    else
    {
        ShowWindow(dialog, SW_SHOW);
    }
    UpdateWindow(dialog);

    MSG msg{};
    while (IsWindow(dialog))
    {
        const BOOL gotMessage = GetMessageW(&msg, nullptr, 0, 0);
        if (gotMessage <= 0)
        {
            if (IsWindow(dialog)) DestroyWindow(dialog);
            break;
        }
        if (!IsDialogMessageW(dialog, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    if (owner)
    {
        EnableWindow(owner, TRUE);
        SetForegroundWindow(owner);
    }

    if (!state.accepted)
        return false;

    result = TrimFunctionName(state.value);
    return !result.empty();
}

bool SaveCurrentFunction()
{
    if (!g_isEditingFunction || g_currentFunctionName.empty())
        return true;

    UserDefinedFunction updated;
    updated.name = g_currentFunctionName;
    updated.blocks = ActiveManager().GetBlocks();
    updated.parents = ActiveManager().GetParents();
    updated.repeatBodies = ActiveManager().GetRepeatBodies();

    if (!SaveUserFunction(updated))
    {
        MessageBoxW(g_mainWindow,
            L"関数を保存できませんでした。保存先フォルダやファイルのアクセス権を確認してください。",
            L"関数の保存エラー", MB_OK | MB_ICONERROR);
        return false;
    }

    const int index = FindUserFunctionIndex(updated.name);
    if (index >= 0)
        g_userFunctions[static_cast<size_t>(index)] = std::move(updated);
    else
        g_userFunctions.push_back(std::move(updated));

    return true;
}

void RefreshFunctionList()
{
    if (!g_createdFuncList)
        return;

    SendMessageW(g_createdFuncList, LB_RESETCONTENT, 0, 0);
    const std::wstring createLabel = L"＋ 新しい関数を作る";
    SendMessageW(g_createdFuncList, LB_ADDSTRING, 0,
        reinterpret_cast<LPARAM>(createLabel.c_str()));

    for (const auto& function : g_userFunctions)
    {
        SendMessageW(g_createdFuncList, LB_ADDSTRING, 0,
            reinterpret_cast<LPARAM>(function.name.c_str()));
    }

    SendMessageW(g_createdFuncList, LB_SETCURSEL, static_cast<WPARAM>(-1), 0);
}

void SetFunctionHelp(const wchar_t* text, bool show)
{
    if (!g_functionHelpLabel)
        return;
    SetWindowTextW(g_functionHelpLabel, text);
    ShowWindow(g_functionHelpLabel, show ? SW_SHOW : SW_HIDE);
}

void CloseBlockPalette()
{
    if (g_blockList)
        ShowWindow(g_blockList, SW_HIDE);
    g_showBlockList = false;

    // ==========================================
    // ★ バックアップしておいた元のX座標に一発で完全に書き戻す！
    // ==========================================
    auto blocks = ActiveManager().GetBlocks();
    for (const auto& pair : g_originalXMap)
    {
        size_t idx = pair.first;
        int originalX = pair.second;

        if (idx < blocks.size())
        {
            Block b = blocks[idx];
            b.x = originalX; // どんなに右に動いていても、元の座標へ一発で完璧に戻す！

            // 万が一、計算ズレでカテゴリーエリア（例: X < 250 など）に入り込んでしまった場合の安全ガード
            if (b.x < 250)
            {
                b.x = 250;
            }

            ActiveManager().UpdateBlockAt(static_cast<int>(idx), b);
        }
    }
    g_originalXMap.clear();   // バックアップをクリア
    g_blockShiftMap.clear();  // シフトマップもクリア
    // ==========================================

    if (g_canvasLeft != 230)
    {
        const int delta = 230 - g_canvasLeft;
        g_canvasLeft = 230;
        g_viewX += delta;
    }
    UpdateView();
    InvalidateEditor();
}

bool OpenUserFunctionEditor(const std::wstring& name)
{
    if (g_isEditingFunction && g_currentFunctionName != name)
    {
        if (!SaveCurrentFunction())
            return false;
    }

    UserDefinedFunction function;
    if (!LoadUserFunction(name, function))
    {
        MessageBoxW(g_mainWindow, L"関数を読み込めませんでした。ファイルが壊れていないか確認してください。",
            L"読み込みエラー", MB_OK | MB_ICONERROR);
        return false;
    }

    if (!g_functionManager.ReplaceBlocks(function.blocks, function.parents, function.repeatBodies))
    {
        MessageBoxW(g_mainWindow, L"関数のブロック構造が正しくないため、編集画面を開けませんでした。",
            L"読み込みエラー", MB_OK | MB_ICONERROR);
        return false;
    }

    g_currentFunctionName = name;
    g_isEditingFunction = true;
    g_activeSidebarTab = 2;
    CloseBlockPalette();
    if (g_createdFuncList) ShowWindow(g_createdFuncList, SW_HIDE);
    for (int i = 0; i < CATEGORY_COUNT; ++i)
        if (g_categoryButtons[i]) ShowWindow(g_categoryButtons[i], SW_SHOW);
    SetFunctionHelp(L"左のカテゴリからブロックを選び、一覧からキャンバスへドラッグします。\n数字の部分をクリックすると値を変更できます。\nDelete：削除 / Ctrl+Z：元に戻す", true);
    UpdateView();
    UpdateButtonColors();
    InvalidateEditor();
    return true;
}

bool CreateUserFunction(HWND owner)
{
    std::wstring name;
    if (!PromptForFunctionName(owner, name))
        return false;

    std::wstring reason;
    if (!IsValidUserFunctionName(name, &reason))
    {
        MessageBoxW(owner, reason.c_str(), L"関数名を確認してください", MB_OK | MB_ICONWARNING);
        return false;
    }

    ReloadUserFunctions();
    if (FindUserFunctionIndex(name) >= 0 ||
        std::filesystem::exists(GetUserFunctionDirectory() / (name + L".blk")))
    {
        MessageBoxW(owner, L"同じ名前の関数がすでにあります。別の名前を入力してください。",
            L"名前が重複しています", MB_OK | MB_ICONWARNING);
        return false;
    }

    UserDefinedFunction function;
    function.name = name;
    if (!SaveUserFunction(function))
    {
        MessageBoxW(owner, L"新しい関数を保存できませんでした。", L"エラー", MB_OK | MB_ICONERROR);
        return false;
    }

    g_userFunctions.push_back(function);
    std::sort(g_userFunctions.begin(), g_userFunctions.end(),
        [](const UserDefinedFunction& a, const UserDefinedFunction& b) { return a.name < b.name; });

    return OpenUserFunctionEditor(name);
}

void ShowFunctionList()
{
    if (g_isEditingFunction && !SaveCurrentFunction())
        return;

    g_isEditingFunction = false;
    g_currentFunctionName.clear();
    g_activeSidebarTab = 1;
    CloseBlockPalette();
    for (int i = 0; i < CATEGORY_COUNT; ++i)
        if (g_categoryButtons[i]) ShowWindow(g_categoryButtons[i], SW_HIDE);

    //関数一覧を表示する際は、メインの可変リスト幅の影響をリセットし、キャンバス位置を初期位置（230）に戻す
    if (g_canvasLeft != 230)
    {
        int delta = 230 - g_canvasLeft;
        g_canvasLeft = 230;
        g_viewX += delta;
    }

    // 関数一覧リスト（g_createdFuncList）の位置と幅を常に初期の固定位置に設定する
    if (g_createdFuncList)
    {
        SetWindowPos(g_createdFuncList, nullptr, 12, 95, 202, 500, SWP_NOZORDER);
        ShowWindow(g_createdFuncList, SW_SHOW);
    }

    ReloadUserFunctions();
    RefreshFunctionList();
    if (g_createdFuncList) ShowWindow(g_createdFuncList, SW_SHOW);
    SetFunctionHelp(L"「＋ 新しい関数を作る」をクリックすると作成できます。\n作成した関数をダブルクリックすると編集できます。\n関数はメイン画面の「関数」カテゴリから呼び出せます。", true);
    UpdateView();
    UpdateButtonColors();
    InvalidateEditor();
}

// ============================================================
// 座標変換を更新
// ============================================================

void UpdateView()
{
    g_zoom = (std::max)(
        MIN_ZOOM,
        (std::min)(g_zoom, MAX_ZOOM)
        );

    ActiveManager().SetViewTransform(
        g_zoom,
        g_viewX,
        g_viewY
    );
}

// ============================================================
// カテゴリ別のブロック一覧（前方宣言）
// ============================================================

std::vector<BlockType> GetBlockTypes(BlockCategory category);

BlockCategory GetCategoryFromBlockType(BlockType type)
{
    for (int i = 0; i < CATEGORY_COUNT; ++i)
    {
        BlockCategory category =
            static_cast<BlockCategory>(i);

        std::vector<BlockType> types =
            GetBlockTypes(category);

        for (BlockType t : types)
        {
            if (t == type)
                return category;
        }
    }

    return BlockCategory::Other;
}
// ============================================================
// カテゴリ別のブロック一覧
// ============================================================

std::vector<BlockType> GetBlockTypes(BlockCategory category)
{
    switch (category)
    {
    case BlockCategory::Move:
        return {
            BlockType::Move,
            BlockType::MoveBackward,
            BlockType::MoveRight,
            BlockType::MoveLeft,
            BlockType::GetAwayFromWall
        };

    case BlockCategory::Control:
        return {
            BlockType::Repeat,
            BlockType::EndRepeat,
            BlockType::Wait,
            BlockType::If,
            BlockType::Else,
            BlockType::EndIf,
            BlockType::DoNothing
        };

    case BlockCategory::Variables:
        return {
            BlockType::CreateVariable,
            BlockType::SetVariable,
            BlockType::ChangeVariable,
            BlockType::AddVariable,
            BlockType::SubtractVariable,
            BlockType::SetBoolean
        };

    case BlockCategory::Function:
        return {
            BlockType::KeepAwayRadarFire,
            BlockType::BodyBlowFire,
            BlockType::SmartFire,
            BlockType::TurnBodyToEnemy,
            BlockType::TurnGunToEnemy,
            BlockType::HesitateNextBullet,
            BlockType::VictoryDance,
            BlockType::VictorySpin,
            BlockType::ForwardBackwardGun360,
            BlockType::ZigzagMove
        };

    case BlockCategory::Event:
        // イベントは RobotEventManager 側で管理されています。
        // 現状の BlockType にはイベント専用ブロックがありません。
        return {};

    case BlockCategory::Rotate:
        return {
            BlockType::TurnRight,
            BlockType::TurnLeft,
            BlockType::TurnGunRight,
            BlockType::TurnGunLeft,
            BlockType::TurnRadarRight,
            BlockType::TurnRadarLeft
        };

    case BlockCategory::Color:
        return {
            BlockType::SetBodyColor,
            BlockType::SetGunColor,
            BlockType::SetRadarColor,
            BlockType::SetBulletColor,
            BlockType::SetScanColor
        };

    case BlockCategory::Information:
        return {
            BlockType::GetName,
            BlockType::GetX,
            BlockType::GetY,
            BlockType::GetEnergy,
            BlockType::GetHeading,
            BlockType::GetGunHeading,
            BlockType::GetRadarHeading,
            BlockType::GetGunCoolingRate,
            BlockType::GetGunHeat,
            BlockType::GetVelocity,
            BlockType::GetOthers,
            BlockType::GetBattleFieldHeight,
            BlockType::GetBattleFieldWidth,
            BlockType::GetHeight,
            BlockType::GetWidth,
            BlockType::GetNumRounds,
            BlockType::GetRoundNum,
            BlockType::GetTime
        };

    case BlockCategory::System:
        return {
            BlockType::Stop,
            BlockType::Resume,
            BlockType::SetAdjustGunForRobotTurn,
            BlockType::SetAdjustRadarForRobotTurn,
            BlockType::SetAdjustRadarForGunTurn
        };

    case BlockCategory::Other:
        return {
            BlockType::FireBullet,
             BlockType::FireBulletAndGetResult,
            BlockType::Scan
        };

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

            // ★ 1. まだバックアップがなければ、リストを開く直前の全員のX座標を完全に保存する
            if (g_originalXMap.empty())
            {
                for (size_t i = 0; i < blocks.size(); ++i)
                {
                    g_originalXMap[i] = blocks[i].x;
                }
            }

            // 2. リストの右端座標を計算し、十分な余裕（例: 45px）を足した目標ラインを決める
            int listScreenRight = ptTopLeft.x + g_listWidth;
            POINT worldListRight = ActiveManager().ScreenToWorld(listScreenRight, CANVAS_TOP);

            int targetMinX = worldListRight.x + 45; // 綺麗な余白

            // 3. 一番左にあるものを基準に、必要なスライド量を求める
            int maxShiftNeeded = 0;
            for (const auto& b : blocks)
            {
                if (b.x < targetMinX)
                {
                    int needed = targetMinX - b.x;
                    if (needed > maxShiftNeeded)
                    {
                        maxShiftNeeded = needed;
                    }
                }
            }

            // 4. 対象ブロック群を綺麗に平行移動させる
            if (maxShiftNeeded > 0)
            {
                for (size_t i = 0; i < blocks.size(); ++i)
                {
                    Block b = blocks[i];
                    if (b.x < targetMinX + 250)
                    {
                        b.x += maxShiftNeeded;
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
        if (g_hoveredBlockListItem != index)
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
        if (g_showBlockList && g_blockList && IsWindowVisible(g_blockList) && abs(p.x - rc.right) <= 6)
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
            g_dragBlockType = g_visibleBlockTypes[static_cast<size_t>(index)];
            g_dragBlockFunctionName.clear();
            if (index < static_cast<LRESULT>(g_visibleFunctionNames.size()))
                g_dragBlockFunctionName = g_visibleFunctionNames[static_cast<size_t>(index)];

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
        if (g_hoveredCategory != index)
        {
            g_hoveredCategory = index;
            InvalidateRect(hwnd, nullptr, FALSE);
        }

        {
            TRACKMOUSEEVENT tme{};
            tme.cbSize = sizeof(tme);
            tme.dwFlags = TME_LEAVE;
            tme.hwndTrack = hwnd;
            TrackMouseEvent(&tme);
        }
        break;

    case WM_MOUSELEAVE:
        if (g_hoveredCategory == index)
        {
            g_hoveredCategory = -1;
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        break;
    }

    return CallWindowProcW(
        g_oldCategoryProcs[index], hwnd, msg, wp, lp);
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

bool SaveProject(HWND hwnd)
{
    if (g_numericEdit) FinishNumericEdit(hwnd, true);
    if (g_isEditingFunction)
    {
        const bool saved = SaveCurrentFunction();
        if (saved)
            MessageBoxW(hwnd, L"関数を保存しました。", L"保存完了", MB_OK | MB_ICONINFORMATION);
        return saved;
    }
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

    if (!BlockFileManager::SaveToFile(
        path,
        ActiveManager().GetBlocks(),
        ActiveManager().GetParents()))
        return false;

    return SaveFunctionCallMetadata(path, ActiveManager().GetBlocks()) &&
        SaveRepeatBodyMetadata(path, ActiveManager().GetRepeatBodies());
}

// ============================================================
// プロジェクト読み込み
// ============================================================

bool LoadProject(HWND hwnd)
{
    if (g_numericEdit) FinishNumericEdit(hwnd, true);
    if (g_isEditingFunction)
    {
        MessageBoxW(hwnd, L"メインの「メイン」タブへ戻ってからプロジェクトを開いてください。関数の中身を誤って置き換えないための案内です。",
            L"プロジェクトを開く", MB_OK | MB_ICONINFORMATION);
        return false;
    }
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

    LoadFunctionCallMetadata(path, loadedBlocks);
    std::vector<int> loadedRepeatBodies;
    LoadRepeatBodyMetadata(path, loadedBlocks.size(), loadedRepeatBodies);

    if (!ActiveManager().ReplaceBlocks(
        loadedBlocks,
        loadedParents,
        loadedRepeatBodies))
    {
        return false;
    }

    InvalidateEditor();
    return true;
}


// ============================================================
// ブロック内の数値入力
// ============================================================

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
static int CALLBACK BrowseCallbackProc(HWND hwnd, UINT uMsg, LPARAM lParam, LPARAM lpData)
{
    if (uMsg == BFFM_INITIALIZED)
    {
        // lpData に渡された初期パスの文字列を設定する
        SendMessage(hwnd, BFFM_SETSELECTIONW, TRUE, lpData);
    }
    return 0;
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
        // 関数専用の管理役は初期ブロックを作らず、空の状態から始める。
        g_functionManager.ReplaceBlocks({}, {});
        ReloadUserFunctions();
        UpdateView();

        // ==========================================
        // 左サイドバー上部に「メイン」「関数」タブを作成
        // ==========================================
        g_tabMainBtn = CreateWindowW(
            L"BUTTON", L"メイン",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            12, 55, 100, 30,
            hwnd, reinterpret_cast<HMENU>(ID_TAB_MAIN),
            GetModuleHandleW(nullptr), nullptr
        );

        g_tabFuncBtn = CreateWindowW(
            L"BUTTON", L"関数",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            114, 55, 100, 30,
            hwnd, reinterpret_cast<HMENU>(ID_TAB_FUNCTION),
            GetModuleHandleW(nullptr), nullptr
        );

        // ==========================================
        //作成した関数一覧用のリストボックスを作成（初期は非表示）
        // ==========================================
        g_createdFuncList = CreateWindowW(
            L"LISTBOX", nullptr,
            WS_CHILD | WS_BORDER | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT,
            12, 95, 202, 500,
            hwnd, reinterpret_cast<HMENU>(ID_CREATED_FUNC_LIST),
            GetModuleHandleW(nullptr), nullptr
        );
        ShowWindow(g_createdFuncList, SW_HIDE);

        g_functionHelpLabel = CreateWindowW(
            L"STATIC",
            L"左のカテゴリからブロックを選び、一覧からキャンバスへドラッグして組み立てます。",
            WS_CHILD | SS_LEFT,
            12, 602, 205, 62,
            hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
        ShowWindow(g_functionHelpLabel, SW_SHOW);

        // 関数を保存したファイルを一覧に読み込む
        ReloadUserFunctions();

        // 左側のカテゴリボタン
        CreateCategoryButtons(hwnd);

        // ブロック一覧
        g_blockList = CreateWindowW(
            L"LISTBOX",
            nullptr,
            WS_CHILD | // SW_VISIBLE を外す
            WS_BORDER | WS_VSCROLL |
            LBS_NOTIFY | LBS_NOINTEGRALHEIGHT |
            LBS_OWNERDRAWFIXED | LBS_HASSTRINGS,
            150, 60, 180, 190,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_BLOCK_LIST)),
            GetModuleHandleW(nullptr),
            nullptr
        );
        // 起動時は隠しておく
        ShowWindow(g_blockList, SW_HIDE);

        InitializeBlockList(g_blockList);

        // リストからキャンバスへドラッグ
        g_oldListProc = reinterpret_cast<WNDPROC>(
            SetWindowLongPtrW(
                g_blockList,
                GWLP_WNDPROC,
                reinterpret_cast<LONG_PTR>(BlockListProc)
            )
            );

        // ====================================================
        // 上部ツールバー（画像表示用ボタン生成）
        // ====================================================
        CreateWindowW(L"BUTTON", L"新規", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 250, 9, 65, 32, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_TOOL_NEW)), GetModuleHandleW(nullptr), nullptr);
        CreateWindowW(L"BUTTON", L"開く", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 320, 9, 65, 32, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_TOOL_OPEN)), GetModuleHandleW(nullptr), nullptr);
        CreateWindowW(L"BUTTON", L"保存", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 390, 9, 65, 32, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_TOOL_SAVE)), GetModuleHandleW(nullptr), nullptr);
        CreateWindowW(L"BUTTON", L"元に戻す", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 460, 9, 65, 32, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_TOOL_UNDO)), GetModuleHandleW(nullptr), nullptr);
        CreateWindowW(L"BUTTON", L"やり直し", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 530, 9, 65, 32, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_TOOL_REDO)), GetModuleHandleW(nullptr), nullptr);
        CreateWindowW(L"BUTTON", L"ロボットにする", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 600, 9, 130, 32, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_TOOL_RUN)), GetModuleHandleW(nullptr), nullptr);
        //ロボット名ラベルと入力欄
        CreateWindowW(L"STATIC", L"ロボット名:", WS_CHILD | WS_VISIBLE, 740, 9, 70, 20, hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
        g_editRobotName = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"MyRobot", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 810, 7, 120, 26, hwnd, reinterpret_cast<HMENU>(ID_EDIT_ROBOT_NAME), GetModuleHandleW(nullptr), nullptr);

        //保存先フォルダのパス入力欄と参照ボタン
        CreateWindowW(L"STATIC", L"保存先:", WS_CHILD | WS_VISIBLE, 940, 9, 50, 20, hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
        g_editFolderPath = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"C:\\robocode\\robots", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 990, 7, 180, 26, hwnd, reinterpret_cast<HMENU>(ID_EDIT_FOLDER_PATH), GetModuleHandleW(nullptr), nullptr);
        CreateWindowW(L"BUTTON", L"参照...", WS_CHILD | WS_VISIBLE, 1180, 7, 60, 28, hwnd, reinterpret_cast<HMENU>(ID_BTN_BROWSE), GetModuleHandleW(nullptr), nullptr);

        // BMP 画像のロード
        g_hBmpNew = (HBITMAP)LoadImageW(nullptr, L"assets/new.bmp", IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
        g_hBmpOpen = (HBITMAP)LoadImageW(nullptr, L"assets/open.bmp", IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
        g_hBmpSave = (HBITMAP)LoadImageW(nullptr, L"assets/save.bmp", IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
        g_hBmpUndo = (HBITMAP)LoadImageW(nullptr, L"assets/undo.bmp", IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
        g_hBmpRedo = (HBITMAP)LoadImageW(nullptr, L"assets/redo.bmp", IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
        g_hBmpRun = (HBITMAP)LoadImageW(nullptr, L"assets/run.bmp", IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);

        UpdateButtonColors();
        return 0; // ← WM_CREATE の最後に 1 回だけ return 0 します
    }

    // ========================================================
    // ボタン描画
    // ========================================================
    case WM_DRAWITEM:
    {
        const DRAWITEMSTRUCT* dis = reinterpret_cast<const DRAWITEMSTRUCT*>(lParam);
        if (!dis) return TRUE;

        int id = static_cast<int>(dis->CtlID);

        if (id == ID_TAB_MAIN || id == ID_TAB_FUNCTION)
        {
            HDC hdc = dis->hDC;
            RECT rc = dis->rcItem;
            bool isMain = (id == ID_TAB_MAIN);
            bool selected = (isMain && g_activeSidebarTab == 0) || (!isMain && g_activeSidebarTab != 0);

            COLORREF bgCol = selected ? RGB(59, 130, 246) : RGB(229, 231, 235); // 選択時は青、非選択時はグレー
            COLORREF fgCol = selected ? RGB(255, 255, 255) : RGB(75, 85, 99);

            HBRUSH brush = CreateSolidBrush(bgCol);
            FillRect(hdc, &rc, brush);
            DeleteObject(brush);

            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, fgCol);

            const wchar_t* label = isMain ? L"メイン" : L"関数";
            DrawTextW(hdc, label, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            return TRUE;
        }

        if (id >= ID_CATEGORY_BASE && id < ID_CATEGORY_BASE + CATEGORY_COUNT)
        {
            DrawCategoryButton(dis, id - ID_CATEGORY_BASE);
            return TRUE;
        }

        if (id == ID_BLOCK_LIST)
        {
            DrawBlockListItem(dis);
            return TRUE;
        }

        // ====================================================
        // 上部ツールバーの BMP 画像ボタン描画処理
        // ====================================================
        if (id >= ID_TOOL_NEW && id <= ID_TOOL_JAVA)
        {
            HDC hdc = dis->hDC;
            RECT rc = dis->rcItem;

            // 対応する HBITMAP を取得
            HBITMAP hBmp = nullptr;
            if (id == ID_TOOL_NEW) hBmp = g_hBmpNew;
            else if (id == ID_TOOL_OPEN) hBmp = g_hBmpOpen;
            else if (id == ID_TOOL_SAVE) hBmp = g_hBmpSave;
            else if (id == ID_TOOL_UNDO) hBmp = g_hBmpUndo;
            else if (id == ID_TOOL_REDO) hBmp = g_hBmpRedo;
            else if (id == ID_TOOL_RUN)  hBmp = g_hBmpRun;

            if (hBmp)
            {
                HDC memDC = CreateCompatibleDC(hdc);
                HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, hBmp);

                BITMAP bm{};
                GetObject(hBmp, sizeof(BITMAP), &bm);

                // 伸縮の品質を向上（クリアに表示）
                SetStretchBltMode(hdc, HALFTONE);

                // 「ロボットにする」も含めて、すべてのボタンを領域いっぱいに全面描画する
                int width = rc.right - rc.left;
                int height = rc.bottom - rc.top;

                StretchBlt(hdc, rc.left, rc.top, width, height, memDC, 0, 0, bm.bmWidth, bm.bmHeight, SRCCOPY);

                SelectObject(memDC, oldBmp);
                DeleteDC(memDC);
            }
            else
            {
                // 画像がない場合の背景描画
                COLORREF bgColor = RGB(45, 55, 72);
                if (dis->itemState & ODS_SELECTED)
                {
                    bgColor = RGB(GetRValue(bgColor) * 3 / 4, GetGValue(bgColor) * 3 / 4, GetBValue(bgColor) * 3 / 4);
                }
                HBRUSH bgBrush = CreateSolidBrush(bgColor);
                FillRect(hdc, &rc, bgBrush);
                DeleteObject(bgBrush);

                // フォールバック（文字表示）
                SetBkMode(hdc, TRANSPARENT);
                SetTextColor(hdc, RGB(255, 255, 255));
                const wchar_t* fallbackText = L"";
                if (id == ID_TOOL_NEW) fallbackText = L"新規";
                else if (id == ID_TOOL_OPEN) fallbackText = L"開く";
                else if (id == ID_TOOL_SAVE) fallbackText = L"保存";
                else if (id == ID_TOOL_UNDO) fallbackText = L"戻す";
                else if (id == ID_TOOL_REDO) fallbackText = L"やり直し";
                else if (id == ID_TOOL_RUN)  fallbackText = L"ロボットにする";

                DrawTextW(hdc, fallbackText, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            }

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
            if (ActiveManager().DeleteSelectedBlock())
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
                ActiveManager().SelectAllBlocks();
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
                if (ActiveManager().Undo())
                    InvalidateEditor();
                return 0;

            case 'Y':
                if (ActiveManager().Redo())
                    InvalidateEditor();
                return 0;

            case 'C':
                ActiveManager().CopySelectedBlock();
                return 0;

            case 'V':
            {
                const auto& blocks = ActiveManager().GetBlocks();
                int index = ActiveManager().GetSelectedBlockIndex();

                if (index >= 0 &&
                    index < static_cast<int>(blocks.size()))
                {
                    int x = blocks[index].x + 30;
                    int y = blocks[index].y + 30;

                    if (ActiveManager().PasteCopiedBlock(x, y))
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

        // ==========================================
        // 「メイン」タブが押されたとき
        // ==========================================
        if (id == ID_TAB_MAIN && notification == BN_CLICKED)
        {
            if (g_isEditingFunction && !SaveCurrentFunction())
                return 0;

            g_isEditingFunction = false;
            g_currentFunctionName.clear();
            g_activeSidebarTab = 0;
            CloseBlockPalette();

            for (int i = 0; i < CATEGORY_COUNT; ++i)
                if (g_categoryButtons[i]) ShowWindow(g_categoryButtons[i], SW_SHOW);
            if (g_createdFuncList) ShowWindow(g_createdFuncList, SW_HIDE);
            SetFunctionHelp(L"左のカテゴリからブロックを選び、一覧からキャンバスへドラッグして組み立てます。", true);

            UpdateView();
            UpdateButtonColors();
            InvalidateEditor();
            return 0;
        }

        // ==========================================
        // 「関数」タブが押されたとき
        // ==========================================
        if (id == ID_TAB_FUNCTION && notification == BN_CLICKED)
        {
            // 関数タブは「関数一覧へ戻る」ボタンとしても働く。
            ShowFunctionList();
            return 0;
        }

        // 関数一覧：先頭をクリックすると新規作成、既存項目はダブルクリックで編集。
        if (id == ID_CREATED_FUNC_LIST)
        {
            const LRESULT selectedIndex = SendMessageW(g_createdFuncList, LB_GETCURSEL, 0, 0);
            if (notification == LBN_SELCHANGE && selectedIndex == 0)
            {
                if (!CreateUserFunction(hwnd) && g_createdFuncList)
                    SendMessageW(g_createdFuncList, LB_SETCURSEL, static_cast<WPARAM>(-1), 0);
                return 0;
            }
            if (notification == LBN_DBLCLK && selectedIndex > 0 &&
                selectedIndex <= static_cast<LRESULT>(g_userFunctions.size()))
            {
                const std::wstring name = g_userFunctions[static_cast<size_t>(selectedIndex - 1)].name;
                OpenUserFunctionEditor(name);
                return 0;
            }
            return 0;
        }

        // 「参照...」ボタンが押されたとき
        if (id == ID_BTN_BROWSE && notification == BN_CLICKED)
        {
            IFileDialog* pFileDialog = nullptr;
            HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pFileDialog));

            if (SUCCEEDED(hr))
            {
                //フォルダ選択モードにするためのフラグを設定
                DWORD dwOptions;
                if (SUCCEEDED(pFileDialog->GetOptions(&dwOptions)))
                {
                    pFileDialog->SetOptions(dwOptions | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
                }

                //最初に開くフォルダを C:\robocode\robots に強制指定
                IShellItem* pDefaultFolder = nullptr;
                std::wstring initialPath = L"C:\\robocode\\robots";

                // フォルダが存在しない場合に備えて自動作成しておく
                std::filesystem::create_directories(initialPath);

                if (SUCCEEDED(SHCreateItemFromParsingName(initialPath.c_str(), NULL, IID_PPV_ARGS(&pDefaultFolder))))
                {
                    pFileDialog->SetFolder(pDefaultFolder);
                    pDefaultFolder->Release();
                }

                // ダイアログのタイトルを設定
                pFileDialog->SetTitle(L"ロボットの保存先フォルダ（パッケージ）を選択してください");

                //ダイアログを表示
                if (SUCCEEDED(pFileDialog->Show(hwnd)))
                {
                    IShellItem* pResult = nullptr;
                    if (SUCCEEDED(pFileDialog->GetResult(&pResult)))
                    {
                        PWSTR pszPath = nullptr;
                        if (SUCCEEDED(pResult->GetDisplayName(SIGDN_FILESYSPATH, &pszPath)))
                        {
                            // 選択されたフォルダのパスをテキストボックスにセット
                            SetWindowTextW(g_editFolderPath, pszPath);
                            CoTaskMemFree(pszPath);
                        }
                        pResult->Release();
                    }
                }
                pFileDialog->Release();
            }
            return 0;
        }

        if (id == 1090 && notification == EN_KILLFOCUS)
        {
            FinishNumericEdit(hwnd, true);
            return 0;
        }

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

            // 新規作成
        case ID_TOOL_NEW:
            if (notification == BN_CLICKED)
            {
                int result = MessageBoxW(
                    hwnd,
                    L"現在のブロックをすべて消去して、新規作成しますか？\n"
                    L"必要な場合は、先に保存してください。",
                    L"新規作成",
                    MB_YESNO | MB_ICONWARNING
                );

                if (result == IDYES)
                {
                    // ここでは誤操作によるデータ消失を防ぐため、
                    // 初期化処理の確認後に実装します。
                    MessageBoxW(
                        hwnd,
                        L"新規作成の初期化処理は、まだ接続されていません。",
                        L"新規作成",
                        MB_OK | MB_ICONINFORMATION
                    );
                }
            }
            return 0;

            // 開く
        case ID_TOOL_OPEN:
            if (notification == BN_CLICKED)
            {
                LoadProject(hwnd);
                InvalidateEditor();
            }
            return 0;

            // 保存
        case ID_TOOL_SAVE:
            if (notification == BN_CLICKED)
            {
                SaveProject(hwnd);
            }
            return 0;

            // 元に戻す
        case ID_TOOL_UNDO:
            if (notification == BN_CLICKED)
            {
                if (ActiveManager().Undo())
                    InvalidateEditor();
            }
            return 0;

            // やり直し
        case ID_TOOL_REDO:
            if (notification == BN_CLICKED)
            {
                if (ActiveManager().Redo())
                    InvalidateEditor();
            }
            return 0;

            // 実行
        case ID_TOOL_RUN:
            if (notification == BN_CLICKED)
            {
                if (g_isEditingFunction)
                {
                    MessageBoxW(hwnd, L"ロボットとして実行するには、先に「メイン」タブへ戻ってください。",
                        L"実行", MB_OK | MB_ICONINFORMATION);
                    return 0;
                }
                const auto& blocks = g_manager.GetBlocks();
                if (blocks.empty())
                {
                    MessageBoxW(hwnd, L"ブロックが配置されていません。", L"警告", MB_OK | MB_ICONWARNING);
                    return 0;
                }

                //画面上の入力欄からテキストを取得
                wchar_t robotName[256]{};
                wchar_t folderPath[MAX_PATH]{};
                GetWindowTextW(g_editRobotName, robotName, 256);
                GetWindowTextW(g_editFolderPath, folderPath, MAX_PATH);

                if (wcslen(robotName) == 0)
                {
                    MessageBoxW(hwnd, L"ロボット名を入力してください。", L"エラー", MB_OK | MB_ICONWARNING);
                    SetFocus(g_editRobotName);
                    return 0;
                }

                // Javaファイル生成・コンパイル・Robocode起動をまとめた関数を呼び出す
                if (ExportJavaCustom(blocks, folderPath, robotName, g_userFunctions))
                {
                    // 成功時の処理（必要であれば）
                }
            }
            return 0;

            // Java出力
        case ID_TOOL_JAVA:
            if (notification == BN_CLICKED)
            {
                MessageBoxW(
                    hwnd,
                    L"Java出力処理は、既存のJava生成処理を確認してから接続します。",
                    L"Java出力",
                    MB_OK | MB_ICONINFORMATION
                );
            }
            return 0;
            // ====================================================
            // キーボードショートカット
            // ====================================================

        case ID_SHORTCUT_SELECT_ALL:
            ActiveManager().SelectAllBlocks();
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
            if (ActiveManager().Undo())
                InvalidateEditor();
            return 0;

        case ID_SHORTCUT_REDO:
            if (ActiveManager().Redo())
                InvalidateEditor();
            return 0;

        case ID_SHORTCUT_COPY:
            ActiveManager().CopySelectedBlock();
            return 0;

        case ID_SHORTCUT_PASTE:
        {
            const auto& blocks = ActiveManager().GetBlocks();
            int index = ActiveManager().GetSelectedBlockIndex();

            if (index >= 0 &&
                index < static_cast<int>(blocks.size()))
            {
                int x = blocks[index].x + 30;
                int y = blocks[index].y + 30;

                if (ActiveManager().PasteCopiedBlock(x, y))
                    InvalidateEditor();
            }

            return 0;
        }


        case ID_SHORTCUT_DELETE:
            if (ActiveManager().DeleteSelectedBlock())
                InvalidateEditor();
            return 0;
        case ID_ADD:
            if (notification == BN_CLICKED)
            {
                POINT center = {
                    g_canvasLeft + 100,
                    CANVAS_TOP + 100
                };

                POINT world =
                    ActiveManager().ScreenToWorld(
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
                        ActiveManager().AddBlock(
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

                ActiveManager().SetDeleteMode(g_deleteMode);

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

            //操作方法ボタンが押されたときのポップアップ処理
        case ID_HELP_BUTTON:
            if (notification == BN_CLICKED)
            {
                MessageBoxW(
                    hwnd,
                    L"【 ロボコード ブロックエディタ 操作ガイド 】\n\n"
                    L"■ ブロックの基本操作\n"
                    L"  ・追加      : リストから項目を選択 ＞ 「追加」ボタン\n"
                    L"  ・移動      : キャンバス上でドラッグ＆ドロップ\n"
                    L"  ・削除      : 「削除モード」をONにしてブロックをクリック\n\n"
                    L"■ カメラ・画面操作\n"
                    L"  ・スクロール: 右クリック ＋ ドラッグ\n"
                    L"  ・拡大/縮小 : マウスホイール回転\n"
                    L"  ・表示リセット: 「表示リセット」ボタン\n\n"
                    L"■ キーボードショートカット\n"
                    L"  ・保存 / 読み込み : Ctrl + S / Ctrl + O\n"
                    L"  ・元に戻す / やり直し: Ctrl + Z / Ctrl + Y\n"
                    L"  ・コピー / 貼り付け : Ctrl + C / Ctrl + V\n"
                    L"  ・全選択 / 削除     : Ctrl + A / Delete",
                    L"操作方法とヘルプ",
                    MB_OK | MB_ICONINFORMATION
                );

            }
            return 0;

        case ID_UNDO:
            if (notification == BN_CLICKED)
            {
                if (ActiveManager().Undo())
                    InvalidateEditor();
            }
            return 0;

        case ID_REDO:
            if (notification == BN_CLICKED)
            {
                if (ActiveManager().Redo())
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

        // リストが開いていて、かつキャンバス境界線付近にマウスがある場合のみカーソルを変更
        if (g_showBlockList && g_blockList && IsWindowVisible(g_blockList))
        {
            if (abs(p.x - g_canvasLeft) <= 6 && p.y >= 50)
            {
                SetCursor(LoadCursor(nullptr, IDC_SIZEWE));
            }
        }

        if (g_isResizingList)
        {
            int newCanvasLeft = p.x;
            if (newCanvasLeft >= 310 && newCanvasLeft <= 500) // 最小・最大の幅制限
            {
                g_canvasLeft = newCanvasLeft;

                // リストの左端をカテゴリーボタンの右側（220px）に固定し、幅を算出
                int listLeft = 220;
                int listWidth = g_canvasLeft - listLeft - 10;
                if (listWidth < 80) listWidth = 80;
                g_listWidth = listWidth;

                // 1. ブロック一覧リストボックスの位置と幅を更新
                if (g_blockList)
                {
                    SetWindowPos(g_blockList, nullptr, listLeft, 60, g_listWidth, 475, SWP_NOZORDER);
                }

                // 2. 関数リストの位置と幅を更新
                if (g_createdFuncList)
                {
                    SetWindowPos(g_createdFuncList, nullptr, listLeft, 95, g_listWidth, 500, SWP_NOZORDER);
                }

                UpdateView();
                InvalidateEditor();
            }
            return 0;
        }

        //リストの右端にマウスが乗っているとき、カーソルを左右の矢印（↔）にする
        if (g_showBlockList && g_blockList && IsWindowVisible(g_blockList))
        {
            RECT listRc;
            GetWindowRect(g_blockList, &listRc);
            ScreenToClient(hwnd, (LPPOINT)&listRc.left);
            ScreenToClient(hwnd, (LPPOINT)&listRc.right);

            if (abs(p.x - listRc.right) <= 6 && p.y >= listRc.top && p.y <= listRc.bottom)
            {
                SetCursor(LoadCursor(nullptr, IDC_SIZEWE));
            }
        }

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

        ActiveManager().OnMouseMove(p.x, p.y);

        // 左ボタンを押している間
        if (wParam & MK_LBUTTON)
        {
            if (ActiveManager().IsSelectingRange())
            {
                // 範囲選択はワールド座標で更新
                POINT world = ActiveManager().ScreenToWorld(
                    p.x,
                    p.y
                );

                ActiveManager().UpdateRangeSelection(
                    world.x,
                    world.y
                );
            }
            else
            {
                // 通常のブロック移動
                ActiveManager().OnMouseMove(p.x, p.y);
            }
        }
        else
        {
            // クリックしていなくてもカーソル位置を更新
            ActiveManager().OnMouseMove(p.x, p.y);
        }

        // カーソルが動くたびに再描画
        InvalidateCanvas();

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


        //リストが開いているとき、その右端（境界線）をクリックしたらリサイズ開始
        if (g_showBlockList && g_blockList && IsWindowVisible(g_blockList))
        {
            // キャンバス左端の境界線
            if (abs(p.x - g_canvasLeft) <= 6 && p.y >= 50)
            {
                g_isResizingList = true;
                SetCapture(hwnd);
                return 0;
            }

            // リストの右端（境界線）
            RECT listRc;
            GetWindowRect(g_blockList, &listRc);
            ScreenToClient(hwnd, (LPPOINT)&listRc.left);
            ScreenToClient(hwnd, (LPPOINT)&listRc.right);

            if (abs(p.x - listRc.right) <= 6 && p.y >= listRc.top && p.y <= listRc.bottom)
            {
                g_isResizingList = true;
                SetCapture(hwnd);
                return 0;
            }
        }

        

        // Java出力ボタン
        if (p.x >= 10 && p.x <= 140 &&
            p.y >= 10 && p.y <= 48)
        {
            if (g_isEditingFunction)
            {
                MessageBoxW(hwnd, L"ロボットとして出力するには、先に「メイン」タブへ戻ってください。",
                    L"Java出力", MB_OK | MB_ICONINFORMATION);
                return 0;
            }
            const auto& blocks = g_manager.GetBlocks();
            if (blocks.empty())
            {
                MessageBoxW(hwnd, L"ブロックが配置されていません。", L"警告", MB_OK | MB_ICONWARNING);
                return 0;
            }

            // 画面のテキストボックスから「ロボット名」と「保存先フォルダ」を取得
            wchar_t robotName[256]{};
            wchar_t folderPath[MAX_PATH]{};
            GetWindowTextW(g_editRobotName, robotName, 256);
            GetWindowTextW(g_editFolderPath, folderPath, MAX_PATH);

            if (wcslen(robotName) == 0)
            {
                MessageBoxW(hwnd, L"ロボット名を入力してください。", L"エラー", MB_OK | MB_ICONWARNING);
                SetFocus(g_editRobotName);
                return 0;
            }

            // 3つの引数を渡して実行
            ExportJavaCustom(blocks, folderPath, robotName, g_userFunctions);
            return 0;
        }

        if (!IsCanvasPoint(p))
            return 0;

        POINT world = ActiveManager().ScreenToWorld(p.x, p.y);

        // 色変更ブロックをクリックした可能性を記録する
        // ========================================================
        // 色変更ブロックの色見本部分をクリックしたか判定
        // ========================================================

        g_colorClickCandidate = false;

        const auto& colorBlocks = ActiveManager().GetBlocks();

        if ((GetKeyState(VK_CONTROL) & 0x8000) == 0)
        {
            for (int i = static_cast<int>(colorBlocks.size()) - 1;
                i >= 0; --i)
            {
                const Block& b = colorBlocks[static_cast<size_t>(i)];

                // ブロックの範囲内か確認
                if (world.x < b.x ||
                    world.x >= b.x + b.w ||
                    world.y < b.y ||
                    world.y >= b.y + b.h)
                {
                    continue;
                }

                // 色変更ブロックか確認
                bool isColorBlock = false;

                switch (b.type)
                {
                case BlockType::SetBodyColor:
                case BlockType::SetGunColor:
                case BlockType::SetRadarColor:
                case BlockType::SetBulletColor:
                case BlockType::SetScanColor:
                    isColorBlock = true;
                    break;

                default:
                    break;
                }

                // 最前面のブロックが色変更ブロックでなければ終了
                if (!isColorBlock)
                    break;

                // 色見本部分をクリックした場合だけ編集対象にする
                if (IsColorEditArea(b, world))
                {
                    g_colorClickCandidate = true;
                    g_colorClickStart = p;
                }

                break;
            }
        }

        // 数値入力ブロックなら数値入力欄を開く
        if (TryStartNumericEdit(hwnd, p, world))
            return 0;
        if (g_numericEdit)
            FinishNumericEdit(hwnd, true);

        const auto& blocks = ActiveManager().GetBlocks();
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
                ActiveManager().DeleteBlockAt(world.x, world.y);
                InvalidateEditor();
            }

            return 0;
        }

        // 空白からドラッグしたら範囲選択
        if (!hitBlock)
        {
            ActiveManager().StartRangeSelection(world.x, world.y);

            SetCapture(hwnd);
            InvalidateEditor();

            return 0;
        }

        // 色変更ブロックをクリックした場合は色を編集する。
        // ただし、Ctrlキーを押しながらなら通常の選択・移動を優先する。
        if ((GetKeyState(VK_CONTROL) & 0x8000) == 0)
        {
            if (TryStartColorEdit(hwnd, world))
                return 0;
        }

        // ブロックをクリックしたら通常の選択・移動
        ActiveManager().OnMouseDown(hwnd, p.x, p.y);

        SetCapture(hwnd);
        InvalidateEditor();

        return 0;
    }


    // ========================================================
    // 左ボタンを離す：ドラッグ・範囲選択終了
    // ========================================================

    case WM_LBUTTONUP:
    {
        if (g_isResizingList)
        {
            g_isResizingList = false;

            if (g_blockList && IsWindowVisible(g_blockList))
            {
                RECT btnRc{};
                GetWindowRect(g_tabFuncBtn, &btnRc);
                POINT ptTopLeft = { btnRc.right + 5, btnRc.top };
                ScreenToClient(hwnd, &ptTopLeft);

                int listScreenRight = ptTopLeft.x + g_listWidth;
                POINT worldListRight = ActiveManager().ScreenToWorld(listScreenRight, CANVAS_TOP);

                int targetMinX = worldListRight.x + 45;

                auto blocks = ActiveManager().GetBlocks();

                // まだバックアップがなければ（念のため）、現在の位置を初期位置として保存しておく
                if (g_originalXMap.empty())
                {
                    for (size_t i = 0; i < blocks.size(); ++i)
                    {
                        g_originalXMap[i] = blocks[i].x - g_blockShiftMap[i]; // 過去の移動分を引いた位置を記録
                    }
                }

                int maxShiftNeeded = 0;
                for (const auto& b : blocks)
                {
                    if (b.x < targetMinX)
                    {
                        int needed = targetMinX - b.x;
                        if (needed > maxShiftNeeded)
                        {
                            maxShiftNeeded = needed;
                        }
                    }
                }

                if (maxShiftNeeded > 0)
                {
                    for (size_t i = 0; i < blocks.size(); ++i)
                    {
                        Block b = blocks[i];
                        if (b.x < targetMinX + 250)
                        {
                            b.x += maxShiftNeeded;
                            blocks[i] = b;
                            ActiveManager().UpdateBlockAt(static_cast<int>(i), b);
                        }
                    }
                }
                UpdateView();
            }

            if (GetCapture() == hwnd) ReleaseCapture();
            InvalidateEditor();
            return 0;
        }

        // 色変更ブロックのクリック操作を判定する
        if (g_colorClickCandidate)
        {
            g_colorClickCandidate = false;

            POINT p = {
                GET_X_LPARAM(lParam),
                GET_Y_LPARAM(lParam)
            };

            int dx = abs(p.x - g_colorClickStart.x);
            int dy = abs(p.y - g_colorClickStart.y);

            // ほとんど移動していない場合だけクリックとみなす
            if (dx <= 4 && dy <= 4)
            {
                POINT world = ActiveManager().ScreenToWorld(
                    p.x, p.y
                );

                if (TryStartColorEdit(hwnd, world))
                {
                    // マウス捕捉を解除して編集終了
                    if (GetCapture() == hwnd)
                        ReleaseCapture();

                    return 0;
                }
            }
        }
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
                POINT world = ActiveManager().ScreenToWorld(
                    p.x,
                    p.y
                );

                // ドロップ位置にブロックを追加。
                // 自作関数の場合は、関数名も一緒に保存する。
                Block newBlock{};
                newBlock.type = g_dragBlockType;
                newBlock.x = world.x;
                newBlock.y = world.y;
                if (!g_dragBlockFunctionName.empty())
                    SetUserFunctionCallName(newBlock, g_dragBlockFunctionName);
                ActiveManager().AddBlock(newBlock);
            }

            g_dragBlockFunctionName.clear();
            if (GetCapture() == hwnd)
                ReleaseCapture();

            InvalidateEditor();

            return 0;
        }

        if (ActiveManager().IsSelectingRange())
        {
            POINT p = {
                GET_X_LPARAM(lParam),
                GET_Y_LPARAM(lParam)
            };

            POINT world = ActiveManager().ScreenToWorld(
                p.x,
                p.y
            );

            // 最終位置を反映
            ActiveManager().UpdateRangeSelection(
                world.x,
                world.y
            );

            ActiveManager().EndRangeSelection();
        }
        else
        {
            POINT p = {
                GET_X_LPARAM(lParam),
                GET_Y_LPARAM(lParam)
            };

            // マウスを離す前にドラッグ対象の番号を取得
            int draggedIndex = ActiveManager().GetDraggedBlockIndex();

            // ドラッグ操作を終了
            ActiveManager().OnMouseUp(hwnd);

            // 黒い部分で離した場合、ブロックと子ブロックを削除
            if (IsBlackArea(p) && draggedIndex >= 0)
            {
                ActiveManager().DeleteBlockAndChildren(draggedIndex);
            }
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
        g_dragBlockFunctionName.clear();
        g_isResizingList = false;

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

        // ① 新しい画面レイアウトを先に描画
        DrawNewLayout(
            memDC,
            rc.right,
            rc.bottom
        );

        // ② キャンバスの範囲
        RECT canvasRect = {
            g_canvasLeft,
            CANVAS_TOP,
            rc.right - 20,
            rc.bottom - 20
        };


        DrawCanvasGrid(memDC, canvasRect);

        // ③ 既存のブロックと選択枠を描画
        ActiveManager().DrawAll(memDC);

        // 関数編集画面であることと、基本操作をキャンバス上にも表示。
        if (g_isEditingFunction)
        {
            RECT functionTitleRect{ g_canvasLeft + 18, 54, (std::min)(static_cast<int>(rc.right) - 24, g_canvasLeft + 620), 78 };
            DrawLabel(memDC, (L"編集中の関数： " + g_currentFunctionName).c_str(),
                functionTitleRect, RGB(45, 55, 72), 16, true);
            RECT functionGuideRect{ g_canvasLeft + 18, 78, (std::min)(static_cast<int>(rc.right) - 24, g_canvasLeft + 930), 100 };
            DrawLabel(memDC, L"左のカテゴリからドラッグして追加 / 数字をクリックして変更 / Deleteで削除 / Ctrl+Zで元に戻す / 上の「関数」で一覧へ",
                functionGuideRect, RGB(72, 82, 98), 12, false);
        }

        // ④ ドラッグ中のブロック名を表示
        if (g_draggingFromList)
        {
            std::wstring name = g_dragBlockFunctionName.empty()
                ? GetBlockName(g_dragBlockType)
                : L"自作関数： " + g_dragBlockFunctionName;

            RECT dragRect = {
                g_listDragPoint.x + 12,
                g_listDragPoint.y + 12,
                g_listDragPoint.x + 180,
                g_listDragPoint.y + 42
            };

            HBRUSH dragBrush = CreateSolidBrush(
                CATEGORY_COLORS[
                    static_cast<int>(g_selectedCategory)
                ]
            );

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

        // ⑤ ズーム表示
        wchar_t zoomText[64]{};

        swprintf_s(
            zoomText,
            L"Zoom: %.0f%%",
            g_zoom * 100.0f
        );

        SetBkMode(memDC, TRANSPARENT);
        SetTextColor(memDC, RGB(45, 55, 72));

        TextOutW(
            memDC,
            g_canvasLeft,
            15,
            zoomText,
            static_cast<int>(wcslen(zoomText))
        );

        // ⑥ 描画結果を画面へ転送
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

        // ⑦ ダブルバッファを解放
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
        if (g_numericEdit) FinishNumericEdit(hwnd, true);
        if (g_isEditingFunction)
            SaveCurrentFunction();
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

        // GDI+ Bitmap オブジェクトの解放
        if (g_hBmpNew) { DeleteObject(g_hBmpNew);   g_hBmpNew = nullptr; }
        if (g_hBmpOpen) { DeleteObject(g_hBmpOpen);  g_hBmpOpen = nullptr; }
        if (g_hBmpSave) { DeleteObject(g_hBmpSave);  g_hBmpSave = nullptr; }
        if (g_hBmpUndo) { DeleteObject(g_hBmpUndo);  g_hBmpUndo = nullptr; }
        if (g_hBmpRedo) { DeleteObject(g_hBmpRedo);  g_hBmpRedo = nullptr; }
        if (g_hBmpRun) { DeleteObject(g_hBmpRun);   g_hBmpRun = nullptr; }

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

    // GDI+ の初期化
    GdiplusStartupInput gdiplusStartupInput;
    GdiplusStartup(&g_gdiplusToken, &gdiplusStartupInput, nullptr);

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

    ShowWindow(hwnd, SW_MAXIMIZE);
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

    // GDI+ の終了処理
    GdiplusShutdown(g_gdiplusToken);

    return static_cast<int>(msg.wParam);
}