#include "BlockManager.h"
#include <cstdlib>

namespace
{
    constexpr int SNAP_DISTANCE = 24;
    constexpr int COLUMN_DISTANCE = 35;
    constexpr int TAB_HEIGHT = 8;
}

// 初期化
void BlockManager::Initialize()
{
    blocks.clear();
    parent.clear();

    blocks.push_back({ BlockType::Move, 100, 120 });
    blocks.push_back({ BlockType::Turn, 100, 220 });
    blocks.push_back({ BlockType::Fire, 100, 320 });

    // -1 は未接続
    parent.assign(blocks.size(), -1);

    selected = -1;
    dragging = false;
    dragStarted = false;
}

// 全ブロックの描画
void BlockManager::DrawAll(HDC hdc) const
{
    for (const auto& block : blocks)
    {
        DrawBlock(hdc, block);
    }
}

// マウス押下
void BlockManager::OnMouseDown(HWND hwnd, int x, int y)
{
    // 手前のブロックから判定
    for (int i = static_cast<int>(blocks.size()) - 1;
        i >= 0; --i)
    {
        if (HitBlock(blocks[i], x, y))
        {
            selected = i;

            offsetX = x - blocks[i].x;
            offsetY = y - blocks[i].y;

            lastX = x;
            lastY = y;

            dragging = true;
            dragStarted = false;

            SetCapture(hwnd);
            return;
        }
    }
}

// マウス移動
void BlockManager::OnMouseMove(int x, int y)
{
    if (!dragging || selected < 0)
    {
        return;
    }

    int dx = x - lastX;
    int dy = y - lastY;

    if (dx == 0 && dy == 0)
    {
        return;
    }

    // 移動開始時に親との接続を解除
    if (!dragStarted)
    {
        parent[selected] = -1;
        dragStarted = true;
    }

    // 選択ブロックと、その下のブロックを移動
    MoveConnectedBlocks(dx, dy);

    lastX = x;
    lastY = y;
}

// マウスを離す
void BlockManager::OnMouseUp(HWND hwnd)
{
    if (dragging && selected >= 0 && dragStarted)
    {
        SnapSelectedBlock();
    }

    dragging = false;
    dragStarted = false;
    selected = -1;

    if (GetCapture() == hwnd)
    {
        ReleaseCapture();
    }
}

// 接続されたブロックをまとめて移動
void BlockManager::MoveConnectedBlocks(int dx, int dy)
{
    for (int i = 0; i < static_cast<int>(blocks.size()); ++i)
    {
        if (i == selected || IsDescendant(i, selected))
        {
            blocks[i].x += dx;
            blocks[i].y += dy;
        }
    }
}

// 指定ブロックが ancestorIndex の子孫か判定
bool BlockManager::IsDescendant(
    int blockIndex,
    int ancestorIndex) const
{
    if (blockIndex < 0 || ancestorIndex < 0)
    {
        return false;
    }

    int current = parent[blockIndex];

    // 親をたどって接続関係を確認
    for (int count = 0;
        current >= 0 &&
        count < static_cast<int>(blocks.size());
        ++count)
    {
        if (current == ancestorIndex)
        {
            return true;
        }

        current = parent[current];
    }

    return false;
}

// 子ブロックが存在するか
bool BlockManager::HasChild(int blockIndex) const
{
    for (int i = 0; i < static_cast<int>(parent.size()); ++i)
    {
        if (parent[i] == blockIndex)
        {
            return true;
        }
    }

    return false;
}

// 子ブロックの番号を取得
int BlockManager::GetChild(int blockIndex) const
{
    for (int i = 0; i < static_cast<int>(parent.size()); ++i)
    {
        if (parent[i] == blockIndex)
        {
            return i;
        }
    }

    return -1;
}

// ブロックを接続位置に吸着させる
void BlockManager::SnapSelectedBlock()
{
    if (selected < 0)
    {
        return;
    }

    Block& moving = blocks[selected];

    int bestTarget = -1;
    int bestDistance = SNAP_DISTANCE + 1;
    int bestY = moving.y;

    for (int i = 0; i < static_cast<int>(blocks.size()); ++i)
    {
        if (i == selected)
        {
            continue;
        }

        // 自分の子孫への接続は禁止
        if (IsDescendant(i, selected))
        {
            continue;
        }

        // 接続先の下に別のブロックがある場合は接続しない
        if (HasChild(i))
        {
            continue;
        }

        const Block& target = blocks[i];

        // 横方向の距離を確認
        if (std::abs(moving.x - target.x) >= COLUMN_DISTANCE)
        {
            continue;
        }

        // target の下端に moving の上端を合わせる
        int expectedY = target.y + target.h - TAB_HEIGHT;
        int distance = std::abs(moving.y - expectedY);

        if (distance <= SNAP_DISTANCE &&
            distance < bestDistance)
        {
            bestTarget = i;
            bestDistance = distance;
            bestY = expectedY;
        }
    }

    if (bestTarget >= 0)
    {
        moving.x = blocks[bestTarget].x;
        moving.y = bestY;

        // 接続先を保存
        parent[selected] = bestTarget;
    }
}

// 結合されたブロックの実行順を取得
std::vector<int> BlockManager::GetExecutionOrder() const
{
    std::vector<int> order;

    // 各接続グループの先頭を探す
    for (int i = 0; i < static_cast<int>(blocks.size()); ++i)
    {
        // 親があるブロックは先頭ではない
        if (parent[i] != -1)
        {
            continue;
        }

        // 子がない単独ブロックは対象外
        if (!HasChild(i))
        {
            continue;
        }

        int current = i;

        // 上から下へ接続順にたどる
        for (int count = 0;
            current != -1 &&
            count < static_cast<int>(blocks.size());
            ++count)
        {
            order.push_back(current);
            current = GetChild(current);
        }
    }

    return order;
}

// ブロック一覧を取得
const std::vector<Block>& BlockManager::GetBlocks() const
{
    return blocks;
}