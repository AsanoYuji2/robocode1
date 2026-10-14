#include "BlockManager.h"

#include <cstdlib>
#include <cmath>
#include <algorithm>

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

    parent.assign(blocks.size(), -1);

    selected = -1;
    dragging = false;
    dragStarted = false;
    deleteMode = false;

    zoom = 1.0f;
    viewX = 350.0f;
    viewY = 0.0f;
}

void BlockManager::SetDeleteMode(bool enabled)
{
    deleteMode = enabled;

    if (enabled)
    {
        dragging = false;
        dragStarted = false;
        selected = -1;
    }
}

bool BlockManager::IsDeleteMode() const
{
    return deleteMode;
}

void BlockManager::SetViewTransform(
    float newZoom,
    float newOffsetX,
    float newOffsetY)
{
    zoom = (std::max)(0.25f, (std::min)(newZoom, 3.0f));
    viewX = newOffsetX;
    viewY = newOffsetY;
}

POINT BlockManager::ScreenToWorld(int x, int y) const
{
    POINT result{};

    result.x = static_cast<LONG>(
        std::lround((x - viewX) / zoom));

    result.y = static_cast<LONG>(
        std::lround((y - viewY) / zoom));

    return result;
}

POINT BlockManager::WorldToScreen(int x, int y) const
{
    POINT result{};

    result.x = static_cast<LONG>(
        std::lround(x * zoom + viewX));

    result.y = static_cast<LONG>(
        std::lround(y * zoom + viewY));

    return result;
}

void BlockManager::DrawAll(HDC hdc) const
{
    int savedDC = SaveDC(hdc);

    if (savedDC == 0)
        return;

    SetGraphicsMode(hdc, GM_ADVANCED);

    XFORM transform{};

    transform.eM11 = zoom;
    transform.eM12 = 0.0f;
    transform.eM21 = 0.0f;
    transform.eM22 = zoom;
    transform.eDx = viewX;
    transform.eDy = viewY;

    SetWorldTransform(hdc, &transform);

    for (const auto& block : blocks)
    {
        DrawBlock(hdc, block);
    }

    RestoreDC(hdc, savedDC);
}

void BlockManager::OnMouseDown(
    HWND hwnd,
    int x,
    int y)
{
    POINT world = ScreenToWorld(x, y);

    // 削除モード
    if (deleteMode)
    {
        DeleteBlockAt(world.x, world.y);
        return;
    }

    // 手前のブロックを優先
    for (int i = static_cast<int>(blocks.size()) - 1;
         i >= 0; --i)
    {
        if (HitBlock(blocks[i], world.x, world.y))
        {
            selected = i;

            offsetX = world.x - blocks[i].x;
            offsetY = world.y - blocks[i].y;

            lastX = world.x;
            lastY = world.y;

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
        return;

    POINT world = ScreenToWorld(x, y);

    int dx = world.x - lastX;
    int dy = world.y - lastY;

    if (dx == 0 && dy == 0)
        return;

    if (!dragStarted)
    {
        parent[selected] = -1;
        dragStarted = true;
    }

    MoveConnectedBlocks(dx, dy);

    lastX = world.x;
    lastY = world.y;
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
        ReleaseCapture();
}

void BlockManager::MoveConnectedBlocks(int dx, int dy)
{
    for (int i = 0;
         i < static_cast<int>(blocks.size());
         ++i)
    {
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
        return false;

    int current = parent[blockIndex];

    for (int count = 0;
         current >= 0 &&
         count < static_cast<int>(blocks.size());
         ++count)
    {
        if (current == ancestorIndex)
            return true;

        current = parent[current];
    }

    return false;
}

bool BlockManager::HasChild(int blockIndex) const
{
    for (int i = 0;
         i < static_cast<int>(parent.size());
         ++i)
    {
        if (parent[i] == blockIndex)
            return true;
    }

    return false;
}

int BlockManager::GetChild(int blockIndex) const
{
    for (int i = 0;
         i < static_cast<int>(parent.size());
         ++i)
    {
        if (parent[i] == blockIndex)
            return i;
    }

    return -1;
}

void BlockManager::SnapSelectedBlock()
{
    if (selected < 0)
        return;

    Block& moving = blocks[selected];

    int bestTarget = -1;
    int bestDistance = SNAP_DISTANCE + 1;
    int bestY = moving.y;

    for (int i = 0;
         i < static_cast<int>(blocks.size());
         ++i)
    {
        if (i == selected)
            continue;

        if (IsDescendant(i, selected))
            continue;

        if (HasChild(i))
            continue;

        const Block& target = blocks[i];

        if (std::abs(moving.x - target.x) >= COLUMN_DISTANCE)
            continue;

        int expectedY =
            target.y + target.h - TAB_HEIGHT;

        int distance =
            std::abs(moving.y - expectedY);

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

        parent[selected] = bestTarget;
    }
}

std::vector<int> BlockManager::GetExecutionOrder() const
{
    std::vector<int> order;

    for (int i = 0;
         i < static_cast<int>(blocks.size());
         ++i)
    {
        if (parent[i] != -1)
            continue;

        if (!HasChild(i))
            continue;

        int current = i;

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

const std::vector<Block>& BlockManager::GetBlocks() const
{
    return blocks;
}

void BlockManager::AddBlock(
    BlockType type,
    int x,
    int y)
{
    blocks.push_back({ type, x, y });
    parent.push_back(-1);
}

bool BlockManager::DeleteBlockAt(int x, int y)
{
    int index = -1;

    for (int i = static_cast<int>(blocks.size()) - 1;
         i >= 0; --i)
    {
        if (HitBlock(blocks[i], x, y))
        {
            index = i;
            break;
        }
    }

    if (index < 0)
        return false;

    for (int i = 0;
         i < static_cast<int>(parent.size());
         ++i)
    {
        if (parent[i] == index)
            parent[i] = -1;
    }

    for (int i = 0;
         i < static_cast<int>(parent.size());
         ++i)
    {
        if (parent[i] > index)
            --parent[i];
    }

    blocks.erase(blocks.begin() + index);
    parent.erase(parent.begin() + index);

    selected = -1;
    dragging = false;
    dragStarted = false;

    return true;
}