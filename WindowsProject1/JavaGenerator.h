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

// 互換用4引数版。親子接続が渡されない場合は、画面上のY座標順で並べます。
// メイン画面から利用する場合は、下の6引数版を使用してください。
bool ExportJavaCustom(
    const std::vector<Block>& blocks,
    const std::wstring& targetFolder,
    const std::wstring& robotName,
    const std::vector<UserDefinedFunction>& functions);

// 推奨版：メインブロックの親子関係とRepeat内部の接続も渡し、
// 画面でつないだ順番どおりにJavaコードへ変換します。
bool ExportJavaCustom(
    const std::vector<Block>& blocks,
    const std::vector<int>& parents,
    const std::vector<int>& repeatBodies,
    const std::wstring& targetFolder,
    const std::wstring& robotName,
    const std::vector<UserDefinedFunction>& functions);
