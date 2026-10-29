#pragma once

#include <filesystem>
#include <cstddef>
#include <string>
#include <vector>
#include "UserFunction.h"

// ユーザー関数を保存するフォルダ（実行ファイルの隣の UserFunctions）。
std::filesystem::path GetUserFunctionDirectory();

// ファイル名として安全な関数名か確認する。
bool IsValidUserFunctionName(const std::wstring& name, std::wstring* reason = nullptr);

// 保存済みの関数をすべて読み込む。
bool LoadUserFunctions(std::vector<UserDefinedFunction>& functions);

// 指定した関数を保存する。
bool SaveUserFunction(const UserDefinedFunction& function);

// 自作関数の保存ファイルを削除する
bool ClearUserFunctionStorage();

// 指定した関数だけを読み込む。
bool LoadUserFunction(const std::wstring& name, UserDefinedFunction& function);

// .blk本体の保存形式に依存せず、自作関数呼び出し名を別ファイルへ保存する。
bool SaveFunctionCallMetadata(const std::wstring& blockFilePath, const std::vector<Block>& blocks);
void LoadFunctionCallMetadata(const std::wstring& blockFilePath, std::vector<Block>& blocks);

// Repeatブロックの内側にあるブロックの位置も別ファイルに保存する。
bool SaveRepeatBodyMetadata(const std::wstring& blockFilePath, const std::vector<int>& repeatBodies);
void LoadRepeatBodyMetadata(const std::wstring& blockFilePath, std::size_t blockCount, std::vector<int>& repeatBodies);

// プロジェクト専用の自作関数を保存・読み込みする
bool SaveProjectUserFunctions(
    const std::wstring& projectFilePath,
    const std::vector<UserDefinedFunction>& functions);

bool LoadProjectUserFunctions(
    const std::wstring& projectFilePath,
    std::vector<UserDefinedFunction>& functions);
