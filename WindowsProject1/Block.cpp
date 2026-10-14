#include "Block.h"

#include <algorithm>
#include <chrono>
#include <thread>
#include <unordered_map>

// ============================================================
// ブロックの色
// ============================================================

COLORREF GetBlockColor(BlockType type)
{
    switch (type)
    {
    case BlockType::Move:
        return RGB(52, 120, 212);

    case BlockType::Turn:
    case BlockType::TurnLeft:
        return RGB(38, 153, 100);

    case BlockType::MoveRight:
        return RGB(0, 180, 200);

    case BlockType::MoveLeft:
        return RGB(0, 140, 160);

    case BlockType::Fire:
        return RGB(218, 133, 39);

    case BlockType::Bark:
        return RGB(140, 80, 180);

    case BlockType::Repeat:
        return RGB(220, 80, 120);

    case BlockType::Wait:
        return RGB(220, 154, 40);

    case BlockType::If:
        return RGB(190, 70, 120);

    case BlockType::Else:
        return RGB(230, 130, 60);

    case BlockType::EndIf:
        return RGB(130, 50, 90);

    case BlockType::CreateVariable:
        return RGB(52, 120, 212);

    case BlockType::SetVariable:
        return RGB(38, 153, 100);

    case BlockType::ChangeVariable:
        return RGB(230, 130, 60);
    }

    return RGB(100, 100, 100);
}

// ============================================================
// ブロック名
// ============================================================

std::wstring GetBlockName(const Block& b)
{
    switch (b.type)
    {
    case BlockType::Move:
        return L"前進する " +
            std::to_wstring(b.distance);

    case BlockType::Turn:
        return L"右に旋回 90度";

    case BlockType::TurnLeft:
        return L"左に旋回 90度";

    case BlockType::MoveRight:
        return L"右に移動 " +
            std::to_wstring(b.distance);

    case BlockType::MoveLeft:
        return L"左に移動 " +
            std::to_wstring(b.distance);

    case BlockType::Fire:
        return L"弾を発射 威力2";

    case BlockType::Bark:
        return L"後退する " +
            std::to_wstring(b.distance);

    case BlockType::Repeat:
        return L"繰り返す " +
            std::to_wstring(b.repeatCount) + L"回";

    case BlockType::Wait:
        return L"指定秒数待つ " +
            std::to_wstring(b.waitSeconds) + L"秒";

    case BlockType::If:
        switch (b.condition)
        {
        case ConditionType::HealthAtLeast:
            return L"もし体力が " +
                std::to_wstring(b.conditionValue) +
                L"以上なら";

        case ConditionType::VariableAtLeast:
            return L"もし " + b.variableName +
                L" が " +
                std::to_wstring(b.conditionValue) +
                L"以上なら";

        case ConditionType::VariableAtMost:
            return L"もし " + b.variableName +
                L" が " +
                std::to_wstring(b.conditionValue) +
                L"以下なら";

        case ConditionType::VariableEqual:
            return L"もし " + b.variableName +
                L" が " +
                std::to_wstring(b.conditionValue) +
                L"なら";

        case ConditionType::VariableNotEqual:
            return L"もし " + b.variableName +
                L" が " +
                std::to_wstring(b.conditionValue) +
                L"以外なら";

        case ConditionType::Always:
        default:
            return L"もし条件を満たしていたら";
        }

    case BlockType::Else:
        return L"でなければ";

    case BlockType::EndIf:
        return L"条件ここまで";

    case BlockType::CreateVariable:
        return L"変数をつくる " +
            b.variableName + L" = " +
            std::to_wstring(b.value);

    case BlockType::SetVariable:
        return b.variableName + L" に " +
            std::to_wstring(b.value) +
            L" を設定する";

    case BlockType::ChangeVariable:
        if (b.value >= 0)
        {
            return b.variableName + L" を " +
                std::to_wstring(b.value) +
                L" 増やす";
        }
        else
        {
            return b.variableName + L" を " +
                std::to_wstring(-static_cast<long long>(b.value)) +
                L" 減らす";
        }
    }

    return L"不明";
}


std::wstring GetBlockName(BlockType type)
{
    Block block;
    block.type = type;

    return GetBlockName(block);
}


// ============================================================
// ブロックの当たり判定
// ============================================================

bool HitBlock(const Block& b, int x, int y)
{
    return
        x >= b.x &&
        x <= b.x + b.w &&
        y >= b.y &&
        y <= b.y + b.h;
}

// ============================================================
// ブロックの描画
// ============================================================

void DrawBlock(HDC hdc, const Block& b)
{
    const int tabH = 8;

    HBRUSH brush =
        CreateSolidBrush(GetBlockColor(b.type));

    HBRUSH oldBrush =
        (HBRUSH)SelectObject(hdc, brush);

    HPEN pen =
        CreatePen(PS_SOLID, 1, RGB(255, 255, 255));

    HPEN oldPen =
        (HPEN)SelectObject(hdc, pen);

    Rectangle(
        hdc,
        b.x,
        b.y + tabH,
        b.x + b.w,
        b.y + b.h
    );

    RECT notch =
    {
        b.x + 24,
        b.y,
        b.x + 64,
        b.y + tabH + 1
    };

    FillRect(
        hdc,
        &notch,
        (HBRUSH)GetStockObject(WHITE_BRUSH)
    );

    RECT tab =
    {
        b.x + 24,
        b.y + b.h - tabH,
        b.x + 64,
        b.y + b.h + 1
    };

    FillRect(hdc, &tab, brush);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(255, 255, 255));

    const std::wstring name = GetBlockName(b);

    RECT textRect =
    {
        b.x + 12,
        b.y + 10,
        b.x + b.w - 8,
        b.y + b.h - 4
    };

    DrawTextW(
        hdc,
        name.c_str(),
        -1,
        &textRect,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE
    );

    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);

    DeleteObject(pen);
    DeleteObject(brush);
}

// ============================================================
// If / Else / EndIf の範囲検索
// ============================================================

struct IfParts
{
    size_t elseIndex;
    size_t endIfIndex;
};

static IfParts FindIfParts(
    const std::vector<Block>& blocks,
    size_t begin,
    size_t end)
{
    int depth = 0;

    IfParts result = { end, end };

    for (size_t i = begin; i < end; ++i)
    {
        if (blocks[i].type == BlockType::If)
        {
            ++depth;
        }
        else if (blocks[i].type == BlockType::EndIf)
        {
            --depth;

            if (depth == 0)
            {
                result.endIfIndex = i;
                return result;
            }
        }
        else if (
            blocks[i].type == BlockType::Else &&
            depth == 1)
        {
            if (result.elseIndex == end)
                result.elseIndex = i;
        }
    }

    return result;
}

// ============================================================
// 条件判定
// ============================================================

using VariableMap =
std::unordered_map<std::wstring, int>;

static bool CheckCondition(
    const Block& block,
    const VariableMap& variables,
    const std::function<bool(const Block&)>& conditionCheck)
{
    switch (block.condition)
    {
    case ConditionType::Always:
        return true;

    case ConditionType::HealthAtLeast:
        // 体力はゲーム側のコールバックで判定
        if (conditionCheck)
            return conditionCheck(block);

        return false;

    case ConditionType::VariableAtLeast:
    case ConditionType::VariableAtMost:
    case ConditionType::VariableEqual:
    case ConditionType::VariableNotEqual:
    {
        const auto it = variables.find(block.variableName);

        // 存在しない変数は条件不成立
        if (it == variables.end())
            return false;

        const int currentValue = it->second;
        const int targetValue = block.conditionValue;

        switch (block.condition)
        {
        case ConditionType::VariableAtLeast:
            return currentValue >= targetValue;

        case ConditionType::VariableAtMost:
            return currentValue <= targetValue;

        case ConditionType::VariableEqual:
            return currentValue == targetValue;

        case ConditionType::VariableNotEqual:
            return currentValue != targetValue;



        default:
            return false;
        }
    }
    }

    return false;
}

// ============================================================
// ブロック列の実行
// ============================================================

static void ExecuteRange(
    const std::vector<Block>& blocks,
    size_t begin,
    size_t end,
    const std::function<void(const Block&)>& executeOne,
    const std::function<bool(const Block&)>& conditionCheck,
    VariableMap& variables)
{
    size_t i = begin;

    while (i < end)
    {
        const Block& block = blocks[i];

        // ----------------------------------------------------
        // 変数をつくる
        // ----------------------------------------------------

        if (block.type == BlockType::CreateVariable)
        {
            // 同名の変数がある場合は初期値で上書き
            variables[block.variableName] = block.value;

            ++i;
            continue;
        }

        // ----------------------------------------------------
        // 変数に値を設定する
        // ----------------------------------------------------

        if (block.type == BlockType::SetVariable)
        {
            variables[block.variableName] = block.value;

            ++i;
            continue;
        }

        // ----------------------------------------------------
        // 変数の値を増減する
        // 正の値なら加算、負の値なら減算
        // ----------------------------------------------------

        if (block.type == BlockType::ChangeVariable)
        {
            int& variable =
                variables[block.variableName];

            // 符号付き整数のオーバーフローを防止
            const long long result =
                static_cast<long long>(variable) +
                static_cast<long long>(block.value);

            variable = static_cast<int>(
                (std::max)(
                    static_cast<long long>(
                        (std::numeric_limits<int>::min)()
                        ),
                    (std::min)(
                        static_cast<long long>(
                            (std::numeric_limits<int>::max)()
                            ),
                        result
                        )
                    )
                );

            ++i;
            continue;
        }

        // ----------------------------------------------------
        // Repeat
        // 現在の仕様ではRepeat以降の範囲全体を繰り返す
        // ----------------------------------------------------

        if (block.type == BlockType::Repeat)
        {
            const int count = (std::max)(
                0,
                (std::min)(block.repeatCount, 1000)
                );

            for (int n = 0; n < count; ++n)
            {
                ExecuteRange(
                    blocks,
                    i + 1,
                    end,
                    executeOne,
                    conditionCheck,
                    variables
                );
            }

            return;
        }

        // ----------------------------------------------------
        // If / Else / EndIf
        // ----------------------------------------------------

        if (block.type == BlockType::If)
        {
            const IfParts parts =
                FindIfParts(blocks, i, end);

            if (parts.endIfIndex == end)
                return;

            const bool conditionMet =
                CheckCondition(
                    block,
                    variables,
                    conditionCheck
                );

            if (conditionMet)
            {
                size_t trueEnd = parts.endIfIndex;

                if (parts.elseIndex != end)
                    trueEnd = parts.elseIndex;

                ExecuteRange(
                    blocks,
                    i + 1,
                    trueEnd,
                    executeOne,
                    conditionCheck,
                    variables
                );
            }
            else if (parts.elseIndex != end)
            {
                ExecuteRange(
                    blocks,
                    parts.elseIndex + 1,
                    parts.endIfIndex,
                    executeOne,
                    conditionCheck,
                    variables
                );
            }

            i = parts.endIfIndex + 1;
            continue;
        }

        // 構造用ブロック自体は実行しない
        if (block.type == BlockType::Else ||
            block.type == BlockType::EndIf)
        {
            ++i;
            continue;
        }

        // ----------------------------------------------------
        // 通常ブロック
        // ----------------------------------------------------

        executeOne(block);

        // ----------------------------------------------------
        // Wait
        // ----------------------------------------------------

        if (block.type == BlockType::Wait)
        {
            const double seconds = (std::max)(
                0.0,
                (std::min)(block.waitSeconds, 3600.0)
                );

            std::this_thread::sleep_for(
                std::chrono::duration<double>(seconds)
            );
        }

        ++i;
    }
}

// ============================================================
// 外部公開関数
// ============================================================

void ExecuteBlockSequence(
    const std::vector<Block>& blocks,
    const std::function<void(const Block&)>& executeOne,
    const std::function<bool(const Block&)>& conditionCheck)
{
    if (!executeOne)
        return;

    // 変数は実行ごとに新しく作る
    VariableMap variables;

    ExecuteRange(
        blocks,
        0,
        blocks.size(),
        executeOne,
        conditionCheck,
        variables
    );
}