#include "Block.h"

#include <algorithm>
#include <chrono>
#include <thread>
#include <limits>

// ============================================================
// ブロック種類の一覧取得
// Block.h の BLOCK_TYPE_LIST(X) から自動生成する
// ============================================================

const std::vector<BlockType>& GetAvailableBlockTypes()
{
    static const std::vector<BlockType> types =
    {
#define ADD_BLOCK(name) BlockType::name,
        BLOCK_TYPE_LIST(ADD_BLOCK)
#undef ADD_BLOCK
    };

    return types;
}

// ============================================================
// ブロックの色
// ============================================================

COLORREF GetBlockColor(BlockType type)
{
    switch (type)
    {
        // 移動の項目
    case BlockType::Move:
    case BlockType::Turn:
    case BlockType::TurnLeft:
    case BlockType::MoveRight:
    case BlockType::MoveLeft:
    case BlockType::Bark:
        return RGB(52, 120, 212);

        // アクション
    case BlockType::Fire:
        return RGB(218, 133, 39);

        // 制御
    case BlockType::EndRepeat:
    case BlockType::Repeat:
    case BlockType::Wait:
    case BlockType::If:
    case BlockType::Else:
    case BlockType::EndIf:
        return RGB(220, 154, 40);

        // 変数
    case BlockType::CreateVariable:
    case BlockType::SetVariable:
    case BlockType::ChangeVariable:
    case BlockType::AddVariable:
    case BlockType::SubtractVariable:
    case BlockType::SetBoolean:
        return RGB(190, 70, 120);
    }

    return RGB(100, 100, 100);
}


// ============================================================
// 条件名
// ============================================================

static std::wstring GetConditionName(
    ConditionType condition,
    const std::wstring& variableName,
    int value,
    const std::wstring& booleanVariableName = L"flag")
{
    switch (condition)
    {
    case ConditionType::Always:
        return L"常に真";

    case ConditionType::HealthAtLeast:
        return L"体力 >= " + std::to_wstring(value);

    case ConditionType::VariableAtLeast:
        return variableName + L" >= " +
            std::to_wstring(value);

    case ConditionType::VariableAtMost:
        return variableName + L" <= " +
            std::to_wstring(value);

    case ConditionType::VariableEqual:
        return variableName + L" == " +
            std::to_wstring(value);

    case ConditionType::VariableNotEqual:
        return variableName + L" != " +
            std::to_wstring(value);

    case ConditionType::VariableGreaterThan:
        return variableName + L" > " +
            std::to_wstring(value);

    case ConditionType::VariableLessThan:
        return variableName + L" < " +
            std::to_wstring(value);

    case ConditionType::EnemyDetected:
        return L"敵を発見";

    case ConditionType::EnemyDistanceAtMost:
        return L"敵との距離 <= " +
            std::to_wstring(value);

    case ConditionType::EnemyOnLeft:
        return L"敵が左側";

    case ConditionType::EnemyOnRight:
        return L"敵が右側";

    case ConditionType::EnemyAhead:
        return L"敵が前方";

    case ConditionType::EnemyBehind:
        return L"敵が後方";

    case ConditionType::WallNear:
        return L"壁との距離 <= " +
            std::to_wstring(value);

    case ConditionType::EnergyAtMost:
        return L"エネルギー <= " +
            std::to_wstring(value);

    case ConditionType::And:
        return L"AND（かつ）";

    case ConditionType::Or:
        return L"OR（または）";

    case ConditionType::Not:
        return L"NOT（反転）";

    case ConditionType::TrueValue:
        return L"真（TRUE）";

    case ConditionType::FalseValue:
        return L"偽（FALSE）";

    case ConditionType::BooleanVariable:
        return booleanVariableName + L" が真";
    }

    return L"不明な条件";
}

// ============================================================
// ブロック名
// ============================================================

std::wstring GetBlockName(const Block& b)
{
    switch (b.type)
    {
    case BlockType::Move:
        return L"前進する  " +
            std::to_wstring(b.distance) + L" 歩";

    case BlockType::Turn:
        return L"右に旋回 90 度";

    case BlockType::TurnLeft:
        return L"左に旋回 90 度";

    case BlockType::MoveRight:
        return L"右に移動  " +
            std::to_wstring(b.distance) + L" 歩";

    case BlockType::MoveLeft:
        return L"左に移動  " +
            std::to_wstring(b.distance) + L" 歩";

    case BlockType::Fire:
        return L"弾を発射 威力2";

    case BlockType::Bark:
        return L"後退する  " +
            std::to_wstring(b.distance) + L" 歩";

    case BlockType::Repeat:
        return L"繰り返す  " +
            std::to_wstring(b.repeatCount) + L" 回";

    case BlockType::EndRepeat:
        return L"繰り返しここまで";

    case BlockType::Wait:
        return L"指定秒数待つ " +
            std::to_wstring(b.waitSeconds) + L"秒";

    case BlockType::If:
        switch (b.condition)
        {
        case ConditionType::And:
            return L"もし (" +
                GetConditionName(
                    b.leftCondition,
                    b.leftVariableName,
                    b.leftConditionValue,
                    b.booleanVariableName
                ) +
                L") AND (" +
                GetConditionName(
                    b.rightCondition,
                    b.rightVariableName,
                    b.rightConditionValue,
                    b.booleanVariableName
                ) + L")";

        case ConditionType::Or:
            return L"もし (" +
                GetConditionName(
                    b.leftCondition,
                    b.leftVariableName,
                    b.leftConditionValue,
                    b.booleanVariableName
                ) +
                L") OR (" +
                GetConditionName(
                    b.rightCondition,
                    b.rightVariableName,
                    b.rightConditionValue,
                    b.booleanVariableName
                ) + L")";

        case ConditionType::Not:
            return L"もし NOT (" +
                GetConditionName(
                    b.leftCondition,
                    b.leftVariableName,
                    b.leftConditionValue,
                    b.booleanVariableName
                ) + L")";

        default:
            return L"もし " +
                GetConditionName(
                    b.condition,
                    b.variableName,
                    b.conditionValue,
                    b.booleanVariableName
                ) + L" なら";
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

        return b.variableName + L" を " +
            std::to_wstring(
                -static_cast<long long>(b.value)
            ) + L" 減らす";

    case BlockType::AddVariable:
        return b.variableName + L" に " +
            std::to_wstring(b.value) + L" を足す";

    case BlockType::SubtractVariable:
        return b.variableName + L" から " +
            std::to_wstring(b.value) + L" を引く";

    case BlockType::SetBoolean:
        return b.booleanVariableName +
            (b.booleanValue
                ? L" = 真（TRUE）"
                : L" = 偽（FALSE）");
    }

    return L"不明";
}

// ============================================================
// ブロック名：種類だけで取得
// ============================================================

std::wstring GetBlockName(BlockType type)
{
    Block block{};
    block.type = type;

    return GetBlockName(block);
}

// ============================================================
// 当たり判定
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
// ブロック描画
// ============================================================

void DrawBlock(HDC hdc, const Block& b)
{
    if (!hdc)
        return;

    const int tabH = 8;

    HBRUSH brush =
        CreateSolidBrush(GetBlockColor(b.type));

    if (!brush)
        return;

    HPEN pen =
        CreatePen(PS_SOLID, 1, RGB(255, 255, 255));

    if (!pen)
    {
        DeleteObject(brush);
        return;
    }

    HGDIOBJ oldBrush = SelectObject(hdc, brush);
    HGDIOBJ oldPen = SelectObject(hdc, pen);

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
        DT_LEFT | DT_VCENTER |
        DT_SINGLELINE | DT_END_ELLIPSIS
    );

    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);

    DeleteObject(pen);
    DeleteObject(brush);
}

// ============================================================
// If / Else / EndIf の対応位置
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
            if (depth == 0)
            {
                result.endIfIndex = i;
                return result;
            }

            --depth;
        }
        else if (
            blocks[i].type == BlockType::Else &&
            depth == 0)
        {
            if (result.elseIndex == end)
                result.elseIndex = i;
        }
    }

    return result;
}

// ============================================================
// Repeat / EndRepeat の対応位置
// ============================================================

struct RepeatParts
{
    size_t endIndex;
};

static RepeatParts FindRepeatParts(
    const std::vector<Block>& blocks,
    size_t begin,
    size_t end)
{
    int depth = 1;

    for (size_t i = begin; i < end; ++i)
    {
        if (blocks[i].type == BlockType::Repeat)
        {
            ++depth;
        }
        else if (blocks[i].type == BlockType::EndRepeat)
        {
            --depth;

            if (depth == 0)
                return { i };
        }
    }

    return { end };
}

// ============================================================
// 整数の安全な加算
// ============================================================

static int SafeAddInt(int current, int amount)
{
    const long long result =
        static_cast<long long>(current) +
        static_cast<long long>(amount);

    const long long minimum =
        (std::numeric_limits<int>::min)();

    const long long maximum =
        (std::numeric_limits<int>::max)();

    return static_cast<int>(
        (std::max)(
            minimum,
            (std::min)(maximum, result)
            )
        );
}

// ============================================================
// 整数の安全な減算
// ============================================================

static int SafeSubtractInt(int current, int amount)
{
    const long long result =
        static_cast<long long>(current) -
        static_cast<long long>(amount);

    const long long minimum =
        (std::numeric_limits<int>::min)();

    const long long maximum =
        (std::numeric_limits<int>::max)();

    return static_cast<int>(
        (std::max)(
            minimum,
            (std::min)(maximum, result)
            )
        );
}

// ============================================================
// 基本条件の判定
// ============================================================

static bool CheckSimpleCondition(
    ConditionType condition,
    const std::wstring& variableName,
    int conditionValue,
    const std::wstring& booleanVariableName,
    const RobotVariables& variables,
    const RobotBooleanVariables& booleanVariables,
    const std::function<bool(const Block&)>& conditionCheck)
{
    switch (condition)
    {
    case ConditionType::Always:
    case ConditionType::TrueValue:
        return true;

    case ConditionType::FalseValue:
        return false;

        // 数値変数の比較
    case ConditionType::VariableAtLeast:
    case ConditionType::VariableAtMost:
    case ConditionType::VariableEqual:
    case ConditionType::VariableNotEqual:
    case ConditionType::VariableGreaterThan:
    case ConditionType::VariableLessThan:
    {
        const auto it = variables.find(variableName);

        if (it == variables.end())
            return false;

        const int current = it->second;

        switch (condition)
        {
        case ConditionType::VariableAtLeast:
            return current >= conditionValue;

        case ConditionType::VariableAtMost:
            return current <= conditionValue;

        case ConditionType::VariableEqual:
            return current == conditionValue;

        case ConditionType::VariableNotEqual:
            return current != conditionValue;

        case ConditionType::VariableGreaterThan:
            return current > conditionValue;

        case ConditionType::VariableLessThan:
            return current < conditionValue;

        default:
            return false;
        }
    }

    // 真偽値変数
    case ConditionType::BooleanVariable:
    {
        const auto it =
            booleanVariables.find(booleanVariableName);

        if (it == booleanVariables.end())
            return false;

        return it->second;
    }

    // 外部センサーによる条件判定
    case ConditionType::HealthAtLeast:
    case ConditionType::EnemyDetected:
    case ConditionType::EnemyDistanceAtMost:
    case ConditionType::EnemyOnLeft:
    case ConditionType::EnemyOnRight:
    case ConditionType::EnemyAhead:
    case ConditionType::EnemyBehind:
    case ConditionType::WallNear:
    case ConditionType::EnergyAtMost:
    {
        if (!conditionCheck)
            return false;

        Block temp{};
        temp.type = BlockType::If;
        temp.condition = condition;
        temp.variableName = variableName;
        temp.conditionValue = conditionValue;

        return conditionCheck(temp);
    }

    case ConditionType::And:
    case ConditionType::Or:
    case ConditionType::Not:
        return false;
    }

    return false;
}

// ============================================================
// 条件判定：AND / OR / NOT
// ============================================================

static bool CheckCondition(
    const Block& block,
    const RobotVariables& variables,
    const RobotBooleanVariables& booleanVariables,
    const std::function<bool(const Block&)>& conditionCheck)
{
    switch (block.condition)
    {
    case ConditionType::And:
    {
        const bool left = CheckSimpleCondition(
            block.leftCondition,
            block.leftVariableName,
            block.leftConditionValue,
            block.booleanVariableName,
            variables,
            booleanVariables,
            conditionCheck
        );

        if (!left)
            return false;

        const bool right = CheckSimpleCondition(
            block.rightCondition,
            block.rightVariableName,
            block.rightConditionValue,
            block.booleanVariableName,
            variables,
            booleanVariables,
            conditionCheck
        );

        return left && right;
    }

    case ConditionType::Or:
    {
        const bool left = CheckSimpleCondition(
            block.leftCondition,
            block.leftVariableName,
            block.leftConditionValue,
            block.booleanVariableName,
            variables,
            booleanVariables,
            conditionCheck
        );

        if (left)
            return true;

        const bool right = CheckSimpleCondition(
            block.rightCondition,
            block.rightVariableName,
            block.rightConditionValue,
            block.booleanVariableName,
            variables,
            booleanVariables,
            conditionCheck
        );

        return left || right;
    }

    case ConditionType::Not:
    {
        const bool result = CheckSimpleCondition(
            block.leftCondition,
            block.leftVariableName,
            block.leftConditionValue,
            block.booleanVariableName,
            variables,
            booleanVariables,
            conditionCheck
        );

        return !result;
    }

    default:
        return CheckSimpleCondition(
            block.condition,
            block.variableName,
            block.conditionValue,
            block.booleanVariableName,
            variables,
            booleanVariables,
            conditionCheck
        );
    }
}

// ============================================================
// ブロック列の内部実行
// ============================================================

static void ExecuteRange(
    const std::vector<Block>& blocks,
    size_t begin,
    size_t end,
    const std::function<void(const Block&)>& executeOne,
    const std::function<bool(const Block&)>& conditionCheck,
    RobotVariables& variables,
    RobotBooleanVariables& booleanVariables)
{
    size_t i = begin;

    while (i < end)
    {
        const Block& block = blocks[i];

        // 数値変数を作成
        if (block.type == BlockType::CreateVariable)
        {
            variables.emplace(
                block.variableName,
                block.value
            );

            ++i;
            continue;
        }

        // 数値変数に値を設定
        if (block.type == BlockType::SetVariable)
        {
            variables[block.variableName] = block.value;

            ++i;
            continue;
        }

        // 真偽値を設定
        if (block.type == BlockType::SetBoolean)
        {
            booleanVariables[block.booleanVariableName] =
                block.booleanValue;

            ++i;
            continue;
        }

        // 変数の増減・足し算・引き算
        if (block.type == BlockType::ChangeVariable ||
            block.type == BlockType::AddVariable ||
            block.type == BlockType::SubtractVariable)
        {
            auto it = variables.find(block.variableName);

            // 未作成の数値変数は変更しない
            if (it != variables.end())
            {
                if (block.type == BlockType::SubtractVariable)
                {
                    it->second = SafeSubtractInt(
                        it->second,
                        block.value
                    );
                }
                else
                {
                    it->second = SafeAddInt(
                        it->second,
                        block.value
                    );
                }
            }

            ++i;
            continue;
        }

        // Repeat
        if (block.type == BlockType::Repeat)
        {
            const RepeatParts parts =
                FindRepeatParts(blocks, i + 1, end);

            // EndRepeat がなければ中断
            if (parts.endIndex == end)
                return;

            const int count = (std::max)(
                0,
                (std::min)(block.repeatCount, 1000)
                );

            for (int n = 0; n < count; ++n)
            {
                ExecuteRange(
                    blocks,
                    i + 1,
                    parts.endIndex,
                    executeOne,
                    conditionCheck,
                    variables,
                    booleanVariables
                );
            }

            i = parts.endIndex + 1;
            continue;
        }

        // 単独の EndRepeat は無視
        if (block.type == BlockType::EndRepeat)
        {
            ++i;
            continue;
        }

        // If
        if (block.type == BlockType::If)
        {
            const IfParts parts =
                FindIfParts(blocks, i + 1, end);

            // EndIf がなければ中断
            if (parts.endIfIndex == end)
                return;

            const bool met = CheckCondition(
                block,
                variables,
                booleanVariables,
                conditionCheck
            );

            if (met)
            {
                const size_t trueEnd =
                    parts.elseIndex != end
                    ? parts.elseIndex
                    : parts.endIfIndex;

                ExecuteRange(
                    blocks,
                    i + 1,
                    trueEnd,
                    executeOne,
                    conditionCheck,
                    variables,
                    booleanVariables
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
                    variables,
                    booleanVariables
                );
            }

            i = parts.endIfIndex + 1;
            continue;
        }

        // 構造用ブロック
        if (block.type == BlockType::Else ||
            block.type == BlockType::EndIf)
        {
            ++i;
            continue;
        }

        // 通常ブロック
        executeOne(block);

        // Wait
        if (block.type == BlockType::Wait)
        {
            const double seconds = (std::max)(
                0.0,
                (std::min)(block.waitSeconds, 3600.0)
                );

            if (seconds > 0.0)
            {
                std::this_thread::sleep_for(
                    std::chrono::duration<double>(seconds)
                );
            }
        }

        ++i;
    }
}

// ============================================================
// ブロック列の実行：変数を新規作成
// ============================================================

void ExecuteBlockSequence(
    const std::vector<Block>& blocks,
    const std::function<void(const Block&)>& executeOne,
    const std::function<bool(const Block&)>& conditionCheck)
{
    RobotVariables variables;
    RobotBooleanVariables booleanVariables;

    ExecuteBlockSequence(
        blocks,
        executeOne,
        conditionCheck,
        variables,
        booleanVariables
    );
}

// ============================================================
// ブロック列の実行：数値変数を共有
// ============================================================

void ExecuteBlockSequence(
    const std::vector<Block>& blocks,
    const std::function<void(const Block&)>& executeOne,
    const std::function<bool(const Block&)>& conditionCheck,
    RobotVariables& variables)
{
    RobotBooleanVariables booleanVariables;

    ExecuteBlockSequence(
        blocks,
        executeOne,
        conditionCheck,
        variables,
        booleanVariables
    );
}

// ============================================================
// ブロック列の実行：数値変数と真偽値変数を共有
// ============================================================

void ExecuteBlockSequence(
    const std::vector<Block>& blocks,
    const std::function<void(const Block&)>& executeOne,
    const std::function<bool(const Block&)>& conditionCheck,
    RobotVariables& variables,
    RobotBooleanVariables& booleanVariables)
{
    if (!executeOne)
        return;

    ExecuteRange(
        blocks,
        0,
        blocks.size(),
        executeOne,
        conditionCheck,
        variables,
        booleanVariables
    );
}

// ============================================================
// イベント名
// ============================================================

std::wstring GetEventName(RobotEvent event)
{
    switch (event)
    {
    case RobotEvent::Start:
        return L"ロボットの実行開始";

    case RobotEvent::EnemyDetected:
        return L"敵を発見したとき";

    case RobotEvent::Damaged:
        return L"被弾したとき";

    case RobotEvent::Hit:
        return L"命中したとき";

    case RobotEvent::Collision:
        return L"衝突したとき";
    }

    return L"不明なイベント";
}

// ============================================================
// イベントの実行：変数を新規作成
// ============================================================

void ExecuteEventSequence(
    const EventHandler& handler,
    const std::function<void(const Block&)>& executeOne,
    const std::function<bool(const Block&)>& conditionCheck)
{
    ExecuteBlockSequence(
        handler.blocks,
        executeOne,
        conditionCheck
    );
}

// ============================================================
// イベントの実行：数値変数を共有
// ============================================================

void ExecuteEventSequence(
    const EventHandler& handler,
    const std::function<void(const Block&)>& executeOne,
    const std::function<bool(const Block&)>& conditionCheck,
    RobotVariables& variables)
{
    ExecuteBlockSequence(
        handler.blocks,
        executeOne,
        conditionCheck,
        variables
    );
}

// ============================================================
// イベントの実行：数値変数と真偽値変数を共有
// ============================================================

void ExecuteEventSequence(
    const EventHandler& handler,
    const std::function<void(const Block&)>& executeOne,
    const std::function<bool(const Block&)>& conditionCheck,
    RobotVariables& variables,
    RobotBooleanVariables& booleanVariables)
{
    ExecuteBlockSequence(
        handler.blocks,
        executeOne,
        conditionCheck,
        variables,
        booleanVariables
    );
}

// ============================================================
// イベント登録のクリア
// ============================================================

void RobotEventManager::Clear()
{
    handlers.clear();
    variables.clear();
    booleanVariables.clear();
}

// ============================================================
// イベントの登録・更新
// ============================================================

void RobotEventManager::RegisterEvent(
    RobotEvent event,
    const std::vector<Block>& blocks)
{
    for (auto& handler : handlers)
    {
        if (handler.event == event)
        {
            handler.blocks = blocks;
            return;
        }
    }

    EventHandler handler;
    handler.event = event;
    handler.blocks = blocks;

    handlers.push_back(handler);
}

// ============================================================
// イベント発生
// ============================================================

void RobotEventManager::TriggerEvent(
    RobotEvent event,
    const std::function<void(const Block&)>& executeOne,
    const std::function<bool(const Block&)>& conditionCheck)
{
    if (!executeOne)
        return;

    for (const auto& handler : handlers)
    {
        if (handler.event == event)
        {
            ExecuteEventSequence(
                handler,
                executeOne,
                conditionCheck,
                variables,
                booleanVariables
            );
        }
    }
}

// ============================================================
// 登録済みイベントの取得
// ============================================================

const std::vector<EventHandler>&
RobotEventManager::GetHandlers() const
{
    return handlers;
}

// ============================================================
// 共有する数値変数の取得
// ============================================================

RobotVariables& RobotEventManager::GetVariables()
{
    return variables;
}

const RobotVariables& RobotEventManager::GetVariables() const
{
    return variables;
}

// ============================================================
// 共有する真偽値変数の取得
// ============================================================

RobotBooleanVariables&
RobotEventManager::GetBooleanVariables()
{
    return booleanVariables;
}

const RobotBooleanVariables&
RobotEventManager::GetBooleanVariables() const
{
    return booleanVariables;
}