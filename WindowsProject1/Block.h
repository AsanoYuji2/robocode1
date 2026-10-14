#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <functional>

enum class BlockType
{
    Move,
    Turn,
    TurnLeft,
    MoveRight,
    MoveLeft,
    Fire,
    Bark,
    Repeat,
    Wait,

    If,              // もし条件を満たしていたら
    Else,            // でなければ
    EndIf,           // 条件ブロックの終わり

    CreateVariable,  // 変数をつくる
    SetVariable,     // 値を設定する
    ChangeVariable   // 値を増減する

};

enum class ConditionType
{
    Always,
    HealthAtLeast,

    VariableAtLeast,    // 変数 >= 指定値
    VariableAtMost,     // 変数 <= 指定値
    VariableEqual,      // 変数 == 指定値
    VariableNotEqual    // 変数 != 指定値
};

struct Block
{
    BlockType type = BlockType::Move;

    int x = 0;
    int y = 0;
    int w = 180;
    int h = 54;

    int distance = 100;
    int repeatCount = 3;
    double waitSeconds = 2.0;

    // 条件分岐
    ConditionType condition = ConditionType::Always;
    int conditionValue = 50;

    // 変数
    std::wstring variableName = L"score";
    int value = 0;

};

COLORREF GetBlockColor(BlockType type);
std::wstring GetBlockName(const Block& block);
bool HitBlock(const Block& block, int x, int y);
void DrawBlock(HDC hdc, const Block& block);

// ブロック列を実行する
void ExecuteBlockSequence(
    const std::vector<Block>& blocks,
    const std::function<void(const Block&)>& executeOne,
    const std::function<bool(const Block&)>& conditionCheck
);


