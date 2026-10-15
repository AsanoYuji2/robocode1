#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <functional>
#include <unordered_map>

// ============================================================
// ブロックの種類
// ============================================================

enum class BlockType
{
    Move,        // 前進する
    Turn,        // 右に90°旋回する
    TurnLeft,    // 左に90°旋回する
    MoveRight,   // 右方向に移動する
    MoveLeft,    // 左方向に移動する
    Fire,        // 弾を発射する
    Bark,        // 後退する

    Repeat,      // 繰り返し処理の開始
    EndRepeat,   // 繰り返し処理の終了
    Wait,        // 指定した秒数だけ待機

    If,          // 条件分岐の開始（条件が真の場合）
    Else,        // 条件が偽の場合の処理
    EndIf,       // 条件分岐の終了

    CreateVariable,   // 数値変数を作成する
    SetVariable,      // 数値変数に値を設定する
    ChangeVariable,   // 数値変数の値を増減する

    // 算術演算
    AddVariable,      // 数値変数に値を足す（加算）
    SubtractVariable, // 数値変数から値を引く（減算）

    // 真偽値の設定
    SetBoolean        // 真偽値変数に真（true）または偽（false）を設定する
};

// ============================================================
// 条件の種類
// ============================================================

enum class ConditionType
{
    Always,
    HealthAtLeast,

    // 数値変数の比較
    VariableAtLeast,
    VariableAtMost,
    VariableEqual,
    VariableNotEqual,
    VariableGreaterThan,
    VariableLessThan,

    // 敵センサー
    EnemyDetected,
    EnemyDistanceAtMost,
    EnemyOnLeft,
    EnemyOnRight,
    EnemyAhead,
    EnemyBehind,

    // 壁センサー
    WallNear,

    // エネルギー
    EnergyAtMost,

    // 論理演算
    And,
    Or,
    Not,

    // 真偽値
    TrueValue,
    FalseValue,
    BooleanVariable
};

// ============================================================
// イベントの種類
// ============================================================

enum class RobotEvent
{
    Start,
    EnemyDetected,
    Damaged,
    Hit,
    Collision
};

// ============================================================
// 変数の種類
// ============================================================

enum class VariableType
{
    Number,
    Boolean
};

// ============================================================
// ブロックデータ
// ============================================================

struct Block
{
    BlockType type = BlockType::Move;

    // 描画位置・サイズ
    int x = 0;
    int y = 0;
    int w = 180;
    int h = 54;

    // 移動距離
    int distance = 100;

    // 繰り返し回数
    int repeatCount = 3;

    // 待機時間
    double waitSeconds = 2.0;

    // 条件分岐
    ConditionType condition = ConditionType::Always;
    int conditionValue = 50;

    // 変数
    std::wstring variableName = L"score";
    int value = 0;

    // 真偽値の設定
    bool booleanValue = true;

    // AND / OR / NOT 用の左側条件
    ConditionType leftCondition = ConditionType::Always;
    std::wstring leftVariableName = L"score";
    int leftConditionValue = 0;

    // AND / OR 用の右側条件
    ConditionType rightCondition = ConditionType::Always;
    std::wstring rightVariableName = L"score";
    int rightConditionValue = 0;

    // NOT や複合条件の真偽値変数
    std::wstring booleanVariableName = L"flag";
};

// ============================================================
// イベントハンドラー
// ============================================================

struct EventHandler
{
    RobotEvent event = RobotEvent::Start;
    std::vector<Block> blocks;
};

// ============================================================
// 変数の管理
// ============================================================

// 数値変数
using RobotVariables =
std::unordered_map<std::wstring, int>;

// 真偽値変数
using RobotBooleanVariables =
std::unordered_map<std::wstring, bool>;

// ============================================================
// ブロック関連
// ============================================================

COLORREF GetBlockColor(BlockType type);

std::wstring GetBlockName(const Block& block);

std::wstring GetBlockName(BlockType type);

bool HitBlock(const Block& block, int x, int y);

void DrawBlock(HDC hdc, const Block& block);

// ============================================================
// ブロック列の実行
// ============================================================

void ExecuteBlockSequence(
    const std::vector<Block>& blocks,
    const std::function<void(const Block&)>& executeOne,
    const std::function<bool(const Block&)>& conditionCheck
);

// 数値変数を共有する実行版
void ExecuteBlockSequence(
    const std::vector<Block>& blocks,
    const std::function<void(const Block&)>& executeOne,
    const std::function<bool(const Block&)>& conditionCheck,
    RobotVariables& variables
);

// 数値変数と真偽値変数を共有する実行版
void ExecuteBlockSequence(
    const std::vector<Block>& blocks,
    const std::function<void(const Block&)>& executeOne,
    const std::function<bool(const Block&)>& conditionCheck,
    RobotVariables& variables,
    RobotBooleanVariables& booleanVariables
);

// ============================================================
// イベント関連
// ============================================================

std::wstring GetEventName(RobotEvent event);

void ExecuteEventSequence(
    const EventHandler& handler,
    const std::function<void(const Block&)>& executeOne,
    const std::function<bool(const Block&)>& conditionCheck
);

// 数値変数を共有するイベント実行版
void ExecuteEventSequence(
    const EventHandler& handler,
    const std::function<void(const Block&)>& executeOne,
    const std::function<bool(const Block&)>& conditionCheck,
    RobotVariables& variables
);

// 数値変数と真偽値変数を共有するイベント実行版
void ExecuteEventSequence(
    const EventHandler& handler,
    const std::function<void(const Block&)>& executeOne,
    const std::function<bool(const Block&)>& conditionCheck,
    RobotVariables& variables,
    RobotBooleanVariables& booleanVariables
);

// ============================================================
// イベント管理クラス
// ============================================================

class RobotEventManager
{
public:
    void Clear();

    void RegisterEvent(
        RobotEvent event,
        const std::vector<Block>& blocks
    );

    void TriggerEvent(
        RobotEvent event,
        const std::function<void(const Block&)>& executeOne,
        const std::function<bool(const Block&)>& conditionCheck
    );

    const std::vector<EventHandler>& GetHandlers() const;

    // 数値変数
    RobotVariables& GetVariables();
    const RobotVariables& GetVariables() const;

    // 真偽値変数
    RobotBooleanVariables& GetBooleanVariables();
    const RobotBooleanVariables& GetBooleanVariables() const;

private:
    std::vector<EventHandler> handlers;

    // イベント間で共有する変数
    RobotVariables variables;
    RobotBooleanVariables booleanVariables;
};
