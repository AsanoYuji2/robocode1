#pragma once

#include <string>
#include <vector>
#include "Block.h"

// ユーザーが作成した関数1つ分のデータ。
// name は画面に表示する名前、blocks / parents は関数の中身。
struct UserDefinedFunction
{
    std::wstring name;
    std::vector<Block> blocks;
    std::vector<int> parents;
    std::vector<int> repeatBodies;
};

// 既存のファイル形式を大きく変えないため、関数呼び出しは
// 既存の DoNothing ブロックの variableName に目印と関数名を保存する。
inline constexpr wchar_t USER_FUNCTION_CALL_PREFIX[] = L"__USER_FUNCTION_CALL__:";

inline bool IsUserFunctionCall(const Block& block)
{
    return block.type == BlockType::DoNothing &&
        block.variableName.rfind(USER_FUNCTION_CALL_PREFIX, 0) == 0;
}

inline std::wstring GetUserFunctionCallName(const Block& block)
{
    if (!IsUserFunctionCall(block))
        return L"";

    return block.variableName.substr(
        sizeof(USER_FUNCTION_CALL_PREFIX) / sizeof(USER_FUNCTION_CALL_PREFIX[0]) - 1);
}

inline void SetUserFunctionCallName(Block& block, const std::wstring& name)
{
    block.type = BlockType::DoNothing;
    block.variableName = USER_FUNCTION_CALL_PREFIX + name;
}
