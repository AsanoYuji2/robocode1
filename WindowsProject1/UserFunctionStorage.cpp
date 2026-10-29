#include "UserFunctionStorage.h"
#include "BlockFileManager.h"

#include <windows.h>
#include <algorithm>
#include <cwctype>
#include <system_error>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace
{
    std::filesystem::path GetFunctionFilePath(const std::wstring& name)
    {
        return GetUserFunctionDirectory() / (name + L".blk");
    }

    std::filesystem::path GetCallMetadataPath(const std::wstring& blockFilePath)
    {
        return std::filesystem::path(blockFilePath + L".calls");
    }

    std::filesystem::path GetRepeatMetadataPath(const std::wstring& blockFilePath)
    {
        return std::filesystem::path(blockFilePath + L".repeat");
    }

    std::string WideToUtf8(const std::wstring& value)
    {
        if (value.empty()) return {};
        const int required = WideCharToMultiByte(CP_UTF8, 0, value.data(),
            static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
        if (required <= 0) return {};
        std::string result(static_cast<size_t>(required), '\0');
        WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
            result.data(), required, nullptr, nullptr);
        return result;
    }

    std::wstring Utf8ToWide(const std::string& value)
    {
        if (value.empty()) return {};
        const int required = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
            value.data(), static_cast<int>(value.size()), nullptr, 0);
        if (required <= 0) return {};
        std::wstring result(static_cast<size_t>(required), L'\0');
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
            static_cast<int>(value.size()), result.data(), required);
        return result;
    }

    bool IsReservedWindowsFileName(const std::wstring& name)
    {
        std::wstring upper;
        upper.reserve(name.size());
        for (wchar_t ch : name)
            upper.push_back(static_cast<wchar_t>(towupper(ch)));

        if (upper == L"CON" || upper == L"PRN" || upper == L"AUX" || upper == L"NUL")
            return true;

        if (upper.size() == 4 &&
            (upper.rfind(L"COM", 0) == 0 || upper.rfind(L"LPT", 0) == 0) &&
            upper[3] >= L'1' && upper[3] <= L'9')
            return true;

        return false;
    }
}

std::filesystem::path GetUserFunctionDirectory()
{
    wchar_t executablePath[MAX_PATH]{};
    DWORD length = GetModuleFileNameW(nullptr, executablePath, MAX_PATH);
    if (length == 0 || length >= MAX_PATH)
        return std::filesystem::current_path() / L"UserFunctions";

    return std::filesystem::path(executablePath).parent_path() / L"UserFunctions";
}


bool ClearUserFunctionStorage()
{
    const auto directory = GetUserFunctionDirectory();
    std::error_code ec;

    if (!std::filesystem::exists(directory, ec))
        return !ec;

    if (ec)
        return false;

    for (std::filesystem::directory_iterator it(directory, ec), end;
        !ec && it != end;
        it.increment(ec))
    {
        if (!it->is_regular_file(ec))
        {
            if (ec)
                return false;

            continue;
        }

        const std::wstring filename =
            it->path().filename().wstring();

        // 自作関数本体と、関数に付随する情報ファイルだけを対象にする
        const bool isFunctionFile =
            it->path().extension() == L".blk" ||
            filename.size() >= 10 &&
            filename.compare(
                filename.size() - 10, 10, L".blk.calls") == 0 ||
            filename.size() >= 11 &&
            filename.compare(
                filename.size() - 11, 11, L".blk.repeat") == 0;

        if (isFunctionFile)
        {
            std::filesystem::remove(it->path(), ec);

            if (ec)
                return false;
        }
    }

    return !ec;
}


bool IsValidUserFunctionName(const std::wstring& name, std::wstring* reason)
{
    auto fail = [&](const wchar_t* message)
    {
        if (reason) *reason = message;
        return false;
    };

    if (name.empty())
        return fail(L"関数名を入力してください。");

    if (name.size() > 80)
        return fail(L"関数名は80文字以内にしてください。");

    if (name == L"." || name == L".." ||
        name.back() == L'.' || name.back() == L' ')
        return fail(L"関数名の最後に「.」や空白は使えません。");

    for (wchar_t ch : name)
    {
        if (ch < 32 || ch == L'<' || ch == L'>' || ch == L':' ||
            ch == L'"' || ch == L'/' || ch == L'\\' || ch == L'|' ||
            ch == L'?' || ch == L'*')
        {
            return fail(L"関数名に「< > : \" / \\ | ? *」は使えません。");
        }
    }

    if (IsReservedWindowsFileName(name))
        return fail(L"この名前はWindowsで予約されているため使えません。");

    if (reason) reason->clear();
    return true;
}

bool LoadUserFunction(const std::wstring& name, UserDefinedFunction& function)
{
    function = UserDefinedFunction{};
    function.name = name;

    std::vector<Block> blocks;
    std::vector<int> parents;
    const std::filesystem::path path = GetFunctionFilePath(name);

    if (!std::filesystem::exists(path))
        return false;

    if (!BlockFileManager::LoadFromFile(path.wstring(), blocks, parents))
        return false;

    LoadFunctionCallMetadata(path.wstring(), blocks);

    if (blocks.size() != parents.size())
        return false;

    std::vector<int> repeatBodies;
    LoadRepeatBodyMetadata(path.wstring(), blocks.size(), repeatBodies);

    function.blocks = std::move(blocks);
    function.parents = std::move(parents);
    function.repeatBodies = std::move(repeatBodies);
    return true;
}

bool SaveUserFunction(const UserDefinedFunction& function)
{
    if (!IsValidUserFunctionName(function.name))
        return false;

    if (function.blocks.size() != function.parents.size() ||
        function.blocks.size() != function.repeatBodies.size())
        return false;

    std::error_code ec;
    std::filesystem::create_directories(GetUserFunctionDirectory(), ec);
    if (ec)
        return false;

    const std::filesystem::path path = GetFunctionFilePath(function.name);
    if (!BlockFileManager::SaveToFile(path.wstring(), function.blocks, function.parents))
        return false;
    if (!SaveFunctionCallMetadata(path.wstring(), function.blocks))
        return false;
    return SaveRepeatBodyMetadata(path.wstring(), function.repeatBodies);
}

bool LoadUserFunctions(std::vector<UserDefinedFunction>& functions)
{
    functions.clear();

    std::error_code ec;
    const auto directory = GetUserFunctionDirectory();
    std::filesystem::create_directories(directory, ec);
    if (ec)
        return false;

    for (std::filesystem::directory_iterator it(directory, ec), end;
         !ec && it != end; it.increment(ec))
    {
        if (!it->is_regular_file(ec) || ec)
        {
            ec.clear();
            continue;
        }

        if (it->path().extension() != L".blk")
            continue;

        const std::wstring name = it->path().stem().wstring();
        if (!IsValidUserFunctionName(name))
            continue;

        UserDefinedFunction function;
        if (LoadUserFunction(name, function))
            functions.push_back(std::move(function));
    }

    std::sort(functions.begin(), functions.end(),
        [](const UserDefinedFunction& left, const UserDefinedFunction& right)
        {
            return left.name < right.name;
        });

    return !ec;
}

bool SaveProjectUserFunctions(
    const std::wstring& projectFilePath,
    const std::vector<UserDefinedFunction>& functions)
{
    const std::filesystem::path directory =
        std::filesystem::path(projectFilePath + L".functions");

    std::error_code ec;

    // このプロジェクト専用の保存フォルダを作り直す
    std::filesystem::remove_all(directory, ec);
    if (ec)
        return false;

    std::filesystem::create_directories(directory, ec);
    if (ec)
        return false;

    for (const UserDefinedFunction& function : functions)
    {
        if (!IsValidUserFunctionName(function.name))
            return false;

        if (function.blocks.size() != function.parents.size() ||
            function.blocks.size() != function.repeatBodies.size())
            return false;

        const std::filesystem::path path =
            directory / (function.name + L".blk");

        if (!BlockFileManager::SaveToFile(
            path.wstring(),
            function.blocks,
            function.parents))
        {
            return false;
        }

        if (!SaveFunctionCallMetadata(
            path.wstring(), function.blocks))
        {
            return false;
        }

        if (!SaveRepeatBodyMetadata(
            path.wstring(), function.repeatBodies))
        {
            return false;
        }
    }

    return true;
}

bool LoadProjectUserFunctions(
    const std::wstring& projectFilePath,
    std::vector<UserDefinedFunction>& functions)
{
    functions.clear();

    const std::filesystem::path directory =
        std::filesystem::path(projectFilePath + L".functions");

    std::error_code ec;

    if (!std::filesystem::exists(directory, ec) || ec)
        return false;

    for (std::filesystem::directory_iterator it(directory, ec), end;
        !ec && it != end;
        it.increment(ec))
    {
        if (!it->is_regular_file(ec) || ec)
        {
            ec.clear();
            continue;
        }

        if (it->path().extension() != L".blk")
            continue;

        const std::wstring name = it->path().stem().wstring();

        if (!IsValidUserFunctionName(name))
            continue;

        UserDefinedFunction function;
        function.name = name;

        if (!BlockFileManager::LoadFromFile(
            it->path().wstring(),
            function.blocks,
            function.parents))
        {
            return false;
        }

        LoadFunctionCallMetadata(
            it->path().wstring(), function.blocks);

        if (function.blocks.size() != function.parents.size())
            return false;

        LoadRepeatBodyMetadata(
            it->path().wstring(),
            function.blocks.size(),
            function.repeatBodies);

        functions.push_back(std::move(function));
    }

    if (ec)
        return false;

    std::sort(
        functions.begin(),
        functions.end(),
        [](const UserDefinedFunction& a,
            const UserDefinedFunction& b)
        {
            return a.name < b.name;
        });

    return true;
}


bool SaveFunctionCallMetadata(const std::wstring& blockFilePath, const std::vector<Block>& blocks)
{
    const std::filesystem::path metadataPath = GetCallMetadataPath(blockFilePath);
    std::ofstream file(metadataPath, std::ios::binary | std::ios::trunc);
    if (!file.is_open())
        return false;

    for (size_t i = 0; i < blocks.size(); ++i)
    {
        if (!IsUserFunctionCall(blocks[i]))
            continue;

        file << i << '\t' << WideToUtf8(GetUserFunctionCallName(blocks[i])) << '\n';
    }

    return file.good();
}

void LoadFunctionCallMetadata(const std::wstring& blockFilePath, std::vector<Block>& blocks)
{
    const std::filesystem::path metadataPath = GetCallMetadataPath(blockFilePath);
    if (!std::filesystem::exists(metadataPath))
        return; // 古いファイルとの互換性を保つ

    // .blk 側に目印が残っていても、サイドカーファイルの内容を正とする。
    for (Block& block : blocks)
    {
        if (IsUserFunctionCall(block))
            block.variableName = L"score";
    }

    std::ifstream file(metadataPath, std::ios::binary);
    if (!file.is_open())
        return;

    std::string line;
    while (std::getline(file, line))
    {
        const size_t tab = line.find('\t');
        if (tab == std::string::npos)
            continue;

        size_t consumed = 0;
        size_t index = 0;
        try
        {
            index = static_cast<size_t>(std::stoull(line.substr(0, tab), &consumed));
        }
        catch (...)
        {
            continue;
        }
        if (consumed != tab || index >= blocks.size())
            continue;

        const std::wstring name = Utf8ToWide(line.substr(tab + 1));
        if (name.empty())
            continue;
        SetUserFunctionCallName(blocks[index], name);
    }
}


bool SaveRepeatBodyMetadata(const std::wstring& blockFilePath, const std::vector<int>& repeatBodies)
{
    const std::filesystem::path metadataPath = GetRepeatMetadataPath(blockFilePath);
    std::ofstream file(metadataPath, std::ios::binary | std::ios::trunc);
    if (!file.is_open())
        return false;

    for (size_t i = 0; i < repeatBodies.size(); ++i)
    {
        if (repeatBodies[i] >= 0)
            file << i << '\t' << repeatBodies[i] << '\n';
    }
    return file.good();
}

void LoadRepeatBodyMetadata(
    const std::wstring& blockFilePath,
    std::size_t blockCount,
    std::vector<int>& repeatBodies)
{
    repeatBodies.assign(blockCount, -1);
    const std::filesystem::path metadataPath = GetRepeatMetadataPath(blockFilePath);
    if (!std::filesystem::exists(metadataPath))
        return;

    std::ifstream file(metadataPath, std::ios::binary);
    if (!file.is_open())
        return;

    std::string line;
    while (std::getline(file, line))
    {
        const size_t tab = line.find('\t');
        if (tab == std::string::npos)
            continue;

        size_t consumedIndex = 0;
        size_t index = 0;
        size_t consumedBody = 0;
        long long bodyValue = -1;
        try
        {
            index = static_cast<size_t>(std::stoull(line.substr(0, tab), &consumedIndex));
            bodyValue = std::stoll(line.substr(tab + 1), &consumedBody);
        }
        catch (...)
        {
            continue;
        }

        if (consumedIndex != tab || consumedBody != line.size() - tab - 1 ||
            index >= repeatBodies.size() || bodyValue < 0 ||
            static_cast<size_t>(bodyValue) >= blockCount)
            continue;

        repeatBodies[index] = static_cast<int>(bodyValue);
    }
}
