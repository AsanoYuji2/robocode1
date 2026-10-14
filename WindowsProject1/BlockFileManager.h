#pragma once

#include <string>
#include <vector>
#include "Block.h"

class BlockFileManager
{
public:
    // 編集データの保存
    static bool SaveToFile(
        const std::wstring& path,
        const std::vector<Block>& blocks,
        const std::vector<int>& parent);

    // 編集データの読み込み
    static bool LoadFromFile(
        const std::wstring& path,
        std::vector<Block>& blocks,
        std::vector<int>& parent);
};