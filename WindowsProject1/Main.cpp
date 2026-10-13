
//#define UNICODE
//#define _UNICODE

#include <windows.h>
#include <windowsx.h>
#include <vector>
#include <string>
#include <fstream>
#include <algorithm>
#include <cstdlib>

enum class BlockType
{
    Move,
    Turn,
    Fire
};

struct Block
{
    BlockType type;
    int x, y;
    int w = 180;
    int h = 54;
};

std::vector<Block> blocks;
int selected = -1;
int dragOffsetX = 0;
int dragOffsetY = 0;
bool dragging = false;

const int SNAP_DISTANCE = 24;
const int TAB_HEIGHT = 8;

COLORREF GetBlockColor(BlockType type)
{
    switch (type)
    {
    case BlockType::Move: return RGB(52, 120, 212);
    case BlockType::Turn: return RGB(38, 153, 100);
    case BlockType::Fire: return RGB(218, 133, 39);
    }
    return RGB(100, 100, 100);
}

std::wstring GetBlockName(BlockType type)
{
    switch (type)
    {
    case BlockType::Move: return L"前進する 100";
    case BlockType::Turn: return L"右に旋回 90度";
    case BlockType::Fire: return L"弾を発射 威力2";
    }
    return L"不明";
}

bool HitBlock(const Block& b, int x, int y)
{
    return x >= b.x && x <= b.x + b.w &&
        y >= b.y && y <= b.y + b.h;
}

void DrawBlock(HDC hdc, const Block& b)
{
    HBRUSH brush = CreateSolidBrush(GetBlockColor(b.type));
    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, brush);

    // ブロック本体
    Rectangle(hdc, b.x, b.y + TAB_HEIGHT,
        b.x + b.w, b.y + b.h);

    // 上側の凹み
    RECT notch = {
        b.x + 24, b.y,
        b.x + 64, b.y + TAB_HEIGHT + 1
    };
    FillRect(hdc, &notch, (HBRUSH)GetStockObject(WHITE_BRUSH));

    // 下側の出っ張り
    RECT tab = {
        b.x + 24, b.y + b.h - TAB_HEIGHT,
        b.x + 64, b.y + b.h + 1
    };
    FillRect(hdc, &tab, brush);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(255, 255, 255));

    RECT textRect = {
        b.x + 12, b.y + 10,
        b.x + b.w - 8, b.y + b.h - 4
    };

    std::wstring name = GetBlockName(b.type);
    DrawTextW(hdc, name.c_str(), -1, &textRect,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    SelectObject(hdc, oldBrush);
    DeleteObject(brush);
}

// Javaソースコードを生成
void ExportJava()
{
    // ブロックの上から下への順序で並べる
    std::vector<Block> ordered = blocks;

    std::sort(ordered.begin(), ordered.end(),
        [](const Block& a, const Block& b)
        {
            if (a.x != b.x)
                return a.x < b.x;
            return a.y < b.y;
        });

    std::string code =
        "import robocode.AdvancedRobot;\n\n"
        "public class BlockRobot extends AdvancedRobot {\n"
        "    @Override\n"
        "    public void run() {\n";

    for (const Block& b : ordered)
    {
        switch (b.type)
        {
        case BlockType::Move:
            code += "        setAhead(100);\n"
                "        execute();\n";
            break;

        case BlockType::Turn:
            code += "        setTurnRight(90);\n"
                "        execute();\n";
            break;

        case BlockType::Fire:
            code += "        setFire(2);\n"
                "        execute();\n";
            break;
        }
    }

    code +=
        "    }\n"
        "}\n";

    std::ofstream file("BlockRobot.java");

    if (file.is_open())
    {
        file << code;
        file.close();

        MessageBoxW(nullptr,
            L"BlockRobot.java を出力しました。",
            L"Java出力", MB_OK | MB_ICONINFORMATION);
    }
    else
    {
        MessageBoxW(nullptr,
            L"ファイルを作成できませんでした。",
            L"エラー", MB_OK | MB_ICONERROR);
    }
}

LRESULT CALLBACK WndProc(
    HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    int mx = GET_X_LPARAM(lp);
    int my = GET_Y_LPARAM(lp);

    switch (msg)
    {
    case WM_CREATE:
        // 初期ブロック
        blocks.push_back({ BlockType::Move, 100, 120 });
        blocks.push_back({ BlockType::Turn, 100, 220 });
        blocks.push_back({ BlockType::Fire, 100, 320 });
        return 0;

    case WM_LBUTTONDOWN:
        // Java出力ボタン
        if (mx >= 10 && mx <= 140 &&
            my >= 10 && my <= 48)
        {
            ExportJava();
            return 0;
        }

        for (int i = (int)blocks.size() - 1; i >= 0; --i)
        {
            if (HitBlock(blocks[i], mx, my))
            {
                selected = i;
                dragOffsetX = mx - blocks[i].x;
                dragOffsetY = my - blocks[i].y;
                dragging = true;

                SetCapture(hwnd);
                break;
            }
        }
        return 0;

    case WM_MOUSEMOVE:
        if (dragging && selected >= 0)
        {
            blocks[selected].x = mx - dragOffsetX;
            blocks[selected].y = my - dragOffsetY;
            InvalidateRect(hwnd, nullptr, TRUE);
        }
        return 0;

    case WM_LBUTTONUP:
        if (dragging && selected >= 0)
        {
            Block& moving = blocks[selected];

            // 近いブロックの下側へ吸着
            for (int i = 0; i < (int)blocks.size(); ++i)
            {
                if (i == selected)
                    continue;

                Block& target = blocks[i];

                bool sameColumn =
                    abs(moving.x - target.x) < 35;

                int expectedY =
                    target.y + target.h - TAB_HEIGHT;

                bool closeEnough =
                    abs(moving.y - expectedY) <= SNAP_DISTANCE;

                if (sameColumn && closeEnough)
                {
                    moving.x = target.x;
                    moving.y = expectedY;
                    break;
                }
            }

            dragging = false;
            selected = -1;
            ReleaseCapture();
            InvalidateRect(hwnd, nullptr, TRUE);
        }
        return 0;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        RECT client;
        GetClientRect(hwnd, &client);
        FillRect(hdc, &client,
            (HBRUSH)GetStockObject(WHITE_BRUSH));

        // 出力ボタン
        RECT button = { 10, 10, 140, 48 };
        HBRUSH buttonBrush =
            CreateSolidBrush(RGB(45, 45, 55));
        FillRect(hdc, &button, buttonBrush);
        DeleteObject(buttonBrush);

        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(255, 255, 255));
        DrawTextW(hdc, L"Java出力", -1, &button,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        for (const Block& b : blocks)
            DrawBlock(hdc, b);

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wp, lp);
}

int WINAPI wWinMain(
    HINSTANCE instance, HINSTANCE, PWSTR, int show)
{
    const wchar_t CLASS_NAME[] = L"MyBlockEditor";

    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = instance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);

    RegisterClassW(&wc);

    HWND hwnd = CreateWindowExW(
        0, CLASS_NAME, L"自作ブロックエディタ",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        900, 650,
        nullptr, nullptr, instance, nullptr);

    if (!hwnd)
        return 0;

    ShowWindow(hwnd, show);

    MSG msg = {};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return (int)msg.wParam;
}
