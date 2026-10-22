#pragma once

#include <vector>
#include <string>
#include "Block.h"
#include "UserFunction.h"

// 既存コードとの互換性を保つ3引数版。
bool ExportJavaCustom(
    const std::vector<Block>& blocks,
    const std::wstring& targetFolder,
    const std::wstring& robotName);

// 自作関数の定義も渡す4引数版。
// Java出力時には関数呼び出し位置へ、その関数のブロックを展開する。
bool ExportJavaCustom(
    const std::vector<Block>& blocks,
    const std::wstring& targetFolder,
    const std::wstring& robotName,
    const std::vector<UserDefinedFunction>& functions);
