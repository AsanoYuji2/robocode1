#include "BlockFileManager.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <utility>

namespace
{
    constexpr char SAVE_MAGIC[8] =
    {
        'B', 'L', 'K', 'E', 'D', 'T', 'R', '1'
    };

    constexpr std::uint32_t SAVE_VERSION = 2;
    constexpr std::uint32_t MAX_BLOCKS = 10000;
    constexpr std::uint32_t MAX_STRING_LENGTH = 4096;

    template<typename T>
    bool WriteValue(std::ofstream& file, const T& value)
    {
        file.write(
            reinterpret_cast<const char*>(&value),
            sizeof(T));

        return static_cast<bool>(file);
    }

    template<typename T>
    bool ReadValue(std::ifstream& file, T& value)
    {
        file.read(
            reinterpret_cast<char*>(&value),
            sizeof(T));

        return static_cast<bool>(file);
    }

    bool WriteString(
        std::ofstream& file,
        const std::wstring& value)
    {
        if (value.size() > MAX_STRING_LENGTH)
            return false;

        const std::uint32_t length =
            static_cast<std::uint32_t>(value.size());

        if (!WriteValue(file, length))
            return false;

        if (length > 0)
        {
            file.write(
                reinterpret_cast<const char*>(value.data()),
                static_cast<std::streamsize>(
                    length * sizeof(wchar_t)));
        }

        return static_cast<bool>(file);
    }

    bool ReadString(
        std::ifstream& file,
        std::wstring& value)
    {
        std::uint32_t length = 0;

        if (!ReadValue(file, length))
            return false;

        if (length > MAX_STRING_LENGTH)
            return false;

        std::wstring temporary(length, L'\0');

        if (length > 0)
        {
            file.read(
                reinterpret_cast<char*>(&temporary[0]),
                static_cast<std::streamsize>(
                    length * sizeof(wchar_t)));

            if (!file)
                return false;
        }

        value = std::move(temporary);
        return true;
    }

    bool IsValidBlockType(std::int32_t value)
    {
        return value >=
            static_cast<std::int32_t>(BlockType::Move)
            && value <=
            static_cast<std::int32_t>(BlockType::OnWin);
    }

    bool IsValidConditionType(std::int32_t value)
    {
        return value >=
            static_cast<std::int32_t>(ConditionType::Always)
            && value <=
            static_cast<std::int32_t>(
                ConditionType::BooleanVariable);
    }

    bool IsValidParentTree(const std::vector<int>& parents)
    {
        const int count = static_cast<int>(parents.size());

        for (int i = 0; i < count; ++i)
        {
            int current = i;

            for (int steps = 0; steps <= count; ++steps)
            {
                current = parents[current];

                if (current == -1)
                    break;

                if (current < 0 || current >= count)
                    return false;

                if (current == i || steps == count)
                    return false;
            }
        }

        return true;
    }
}

bool BlockFileManager::SaveToFile(
    const std::wstring& path,
    const std::vector<Block>& blocks,
    const std::vector<int>& parent)
{
    if (blocks.size() != parent.size())
        return false;

    if (blocks.size() > MAX_BLOCKS)
        return false;

    std::ofstream file(path, std::ios::binary);

    if (!file)
        return false;

    file.write(SAVE_MAGIC, sizeof(SAVE_MAGIC));

    if (!file)
        return false;

    const std::uint32_t version = SAVE_VERSION;
    const std::uint32_t count =
        static_cast<std::uint32_t>(blocks.size());

    if (!WriteValue(file, version) ||
        !WriteValue(file, count))
        return false;

    for (std::size_t i = 0; i < blocks.size(); ++i)
    {
        const Block& b = blocks[i];

        const std::int32_t type =
            static_cast<std::int32_t>(b.type);

        const std::int32_t condition =
            static_cast<std::int32_t>(b.condition);

        const std::int32_t p =
            static_cast<std::int32_t>(parent[i]);

        if (!WriteValue(file, type) ||
            !WriteValue(file, static_cast<std::int32_t>(b.x)) ||
            !WriteValue(file, static_cast<std::int32_t>(b.y)) ||
            !WriteValue(file, static_cast<std::int32_t>(b.w)) ||
            !WriteValue(file, static_cast<std::int32_t>(b.h)) ||
            !WriteValue(file, static_cast<std::int32_t>(b.distance)) ||
            !WriteValue(file, static_cast<std::int32_t>(b.repeatCount)) ||
            !WriteValue(file, b.waitSeconds) ||
            !WriteValue(file, condition) ||
            !WriteValue(file, static_cast<std::int32_t>(b.conditionValue)) ||
            !WriteString(file, b.variableName) ||
            !WriteValue(file, static_cast<std::int32_t>(b.value)) ||
            !WriteValue(file, p) ||
            !WriteValue(file, b.degrees) ||
            !WriteValue(file, b.power) ||
            !WriteValue(file, static_cast<std::uint32_t>(b.bodyColor)) ||
            !WriteValue(file, static_cast<std::uint32_t>(b.gunColor)) ||
            !WriteValue(file, static_cast<std::uint32_t>(b.radarColor)) ||
            !WriteValue(file, static_cast<std::uint32_t>(b.bulletColor)) ||
            !WriteValue(file, static_cast<std::uint32_t>(b.scanColor)))
        {
            return false;
        }
    }

    file.flush();
    return static_cast<bool>(file);
}

bool BlockFileManager::LoadFromFile(
    const std::wstring& path,
    std::vector<Block>& blocks,
    std::vector<int>& parent)
{
    std::ifstream file(path, std::ios::binary);

    if (!file)
        return false;

    char magic[sizeof(SAVE_MAGIC)] = {};

    file.read(magic, sizeof(magic));

    if (!file ||
        !std::equal(
            std::begin(magic),
            std::end(magic),
            std::begin(SAVE_MAGIC)))
    {
        return false;
    }

    std::uint32_t version = 0;
    std::uint32_t count = 0;

    if (!ReadValue(file, version) ||
        !ReadValue(file, count))
        return false;

    if (version != SAVE_VERSION || count > MAX_BLOCKS)
        return false;

    // 一時領域に読み込む
    std::vector<Block> newBlocks;
    std::vector<int> newParents;

    newBlocks.reserve(count);
    newParents.reserve(count);

    for (std::uint32_t i = 0; i < count; ++i)
    {
        Block b{};

        std::int32_t type = 0;
        std::int32_t condition = 0;

        std::int32_t x = 0;
        std::int32_t y = 0;
        std::int32_t w = 0;
        std::int32_t h = 0;

        std::int32_t distance = 0;
        std::int32_t repeatCount = 0;
        std::int32_t conditionValue = 0;
        std::int32_t value = 0;
        std::int32_t p = -1;
        double degrees = 0.0;
        double power = 1.0;
        std::uint32_t bodyColor = 0;
        std::uint32_t gunColor = 0;
        std::uint32_t radarColor = 0;
        std::uint32_t bulletColor = 0;
        std::uint32_t scanColor = 0;

        if (!ReadValue(file, type) ||
            !ReadValue(file, x) ||
            !ReadValue(file, y) ||
            !ReadValue(file, w) ||
            !ReadValue(file, h) ||
            !ReadValue(file, distance) ||
            !ReadValue(file, repeatCount) ||
            !ReadValue(file, b.waitSeconds) ||
            !ReadValue(file, condition) ||
            !ReadValue(file, conditionValue) ||
            !ReadString(file, b.variableName) ||
            !ReadValue(file, value) ||
            !ReadValue(file, p) ||
            !ReadValue(file, degrees) ||
            !ReadValue(file, power) ||
            !ReadValue(file, bodyColor) ||
            !ReadValue(file, gunColor) ||
            !ReadValue(file, radarColor) ||
            !ReadValue(file, bulletColor) ||
            !ReadValue(file, scanColor))
        {
            return false;
        }

        if (!IsValidBlockType(type) ||
            !IsValidConditionType(condition))
            return false;

        if (w <= 0 || h <= 0 ||
            w > 100000 || h > 100000)
            return false;

        if (!std::isfinite(b.waitSeconds) ||
            b.waitSeconds < 0.0 ||
            b.waitSeconds > 3600.0)
            return false;

        if (repeatCount < 0 || repeatCount > 1000)
            return false;

        if (p < -1 ||
            p >= static_cast<std::int32_t>(count) ||
            p == static_cast<std::int32_t>(i))
            return false;

        b.type = static_cast<BlockType>(type);
        b.condition = static_cast<ConditionType>(condition);

        b.x = static_cast<int>(x);
        b.y = static_cast<int>(y);
        b.w = static_cast<int>(w);
        b.h = static_cast<int>(h);

        b.distance = static_cast<int>(distance);
        b.repeatCount = static_cast<int>(repeatCount);
        b.conditionValue = static_cast<int>(conditionValue);
        b.value = static_cast<int>(value);
        b.degrees = degrees;
        b.power = power;
        b.bodyColor = static_cast<COLORREF>(bodyColor);
        b.gunColor = static_cast<COLORREF>(gunColor);
        b.radarColor = static_cast<COLORREF>(radarColor);
        b.bulletColor = static_cast<COLORREF>(bulletColor);
        b.scanColor = static_cast<COLORREF>(scanColor);

        newBlocks.push_back(std::move(b));
        newParents.push_back(static_cast<int>(p));
    }

    if (!file || !IsValidParentTree(newParents))
        return false;

    // 読み込み成功時だけ既存データを置き換える
    blocks = std::move(newBlocks);
    parent = std::move(newParents);

    return true;
}