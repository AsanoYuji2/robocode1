#include "BlockManager.h"
#include <cstdlib>

namespace
{
    constexpr int SNAP_DISTANCE = 24;
    constexpr int COLUMN_DISTANCE = 35;
    constexpr int TAB_HEIGHT = 8;
}

void BlockManager::Initialize()
{
    blocks.clear();
    parent.clear();

    blocks.push_back({ BlockType::Move, 100, 120 });
    blocks.push_back({ BlockType::Turn, 100, 220 });
    blocks.push_back({ BlockType::Fire, 100, 320 });

    // 最初はすべて独立したブロック
    parent.resize(blocks.size(), -1);

    selected = -1;
    dragging = false;
    dragStarted = false;
}

void BlockManager::DrawAll(HDC hdc) const
{
    for (const auto& b : blocks)
    {
        DrawBlock(hdc, b);
    }
}

void BlockManager::OnMouseDown(HWND hwnd, int x, int y)
{
    // 手前に描画されているブロックから判定
    for (int i = static_cast<int>(blocks.size()) - 1; i >= 0; --i)
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

void BlockManager::OnMouseMove(int x, int y)
{
    if (!dragging || selected < 0)
    {
        return;
    }

    int dx = x - lastX;
    int dy = y - lastY;

    // 実際に移動したときだけ、元の接続を解除する
    if (!dragStarted && (dx != 0 || dy != 0))
    {
        parent[selected] = -1;
        dragStarted = true;
    }

    if (dragStarted)
    {
        // 選択したブロックと、その下につながるブロックを移動
        MoveConnectedBlocks(dx, dy);
    }

    lastX = x;
    lastY = y;
}

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

void BlockManager::MoveConnectedBlocks(int dx, int dy)
{
    for (int i = 0; i < static_cast<int>(blocks.size()); ++i)
    {
        // 選択ブロック自身、またはその子孫を移動する
        if (i == selected || IsDescendant(i, selected))
        {
            blocks[i].x += dx;
            blocks[i].y += dy;
        }
    }
}

bool BlockManager::IsDescendant(
    int blockIndex,
    int ancestorIndex) const
{
    if (blockIndex < 0 || ancestorIndex < 0)
    {
        return false;
    }

    int current = parent[blockIndex];

    // 親をたどり、ancestorIndex が見つかれば子孫
    // 無限ループ防止のため最大 blocks.size() 回まで
    for (int count = 0;
        current >= 0 && count < static_cast<int>(blocks.size());
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

        // 自分の下につながっているブロックには接続しない
        // これにより循環接続を防ぐ
        if (IsDescendant(i, selected))
        {
            continue;
        }

        // 接続先に別の子ブロックがある場合は接続しない
        if (HasChild(i))
        {
            continue;
        }

        const Block& target = blocks[i];

        // 横方向の位置が近いか
        bool sameColumn =
            std::abs(moving.x - target.x) < COLUMN_DISTANCE;

        if (!sameColumn)
        {
            continue;
        }

        // target の下端に moving の上端を合わせる
        // タブの高さ分だけ重ねる
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
        // 座標を合わせ、接続関係を保存
        moving.x = blocks[bestTarget].x;
        moving.y = bestY;

        parent[selected] = bestTarget;
    }
}

const std::vector<Block>& BlockManager::GetBlocks() const
{
    return blocks;
}