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
    Attack,
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
    L"攻撃",
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
    RGB(218, 133, 39),   // 攻撃
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


// 分割ファイル: Functions/UserFunctionFunctions.inl
#include "Functions/UserFunctionFunctions.inl"

// 分割ファイル: UI/ViewUpdate.inl
#include "UI/ViewUpdate.inl"

// 分割ファイル: Blocks/BlockPalette.inl
#include "Blocks/BlockPalette.inl"

// 分割ファイル: UI/BlockList.inl
#include "UI/BlockList.inl"

// 分割ファイル: UI/CategorySelection.inl
#include "UI/CategorySelection.inl"

// 分割ファイル: Editor/BlockListInput.inl
#include "Editor/BlockListInput.inl"

// 分割ファイル: UI/CategoryDrawing.inl
#include "UI/CategoryDrawing.inl"

// 分割ファイル: Project/ProjectFileOperations.inl
#include "Project/ProjectFileOperations.inl"

// 分割ファイル: Editor/NumericInput.inl
#include "Editor/NumericInput.inl"

// 分割ファイル: Editor/BlockValueEditing.inl
#include "Editor/BlockValueEditing.inl"

// 分割ファイル: UI/CanvasDrawing.inl
#include "UI/CanvasDrawing.inl"

// 分割ファイル: UI/FileDialogCallbacks.inl
#include "UI/FileDialogCallbacks.inl"

// 分割ファイル: Window/WindowProcedure.inl
#include "Window/WindowProcedure.inl"

// 分割ファイル: Window/ApplicationEntry.inl
#include "Window/ApplicationEntry.inl"
