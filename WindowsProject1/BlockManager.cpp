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

    undoStack.clear();
    redoStack.clear();

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

    for (int i = 0; i < static_cast<int>(blocks.size()); ++i)
    {
        const Block& block = blocks[i];

        // 通常のブロック描画
        DrawBlock(hdc, block);

        // 選択中のブロックには青い枠を描画
        if (i == selected)
        {
            HPEN pen = CreatePen(
                PS_SOLID,
                3,
                RGB(40, 150, 255)
            );

            HGDIOBJ oldPen = SelectObject(hdc, pen);
            HGDIOBJ oldBrush = SelectObject(
                hdc,
                GetStockObject(HOLLOW_BRUSH)
            );

            Rectangle(
                hdc,
                block.x - 3,
                block.y - 3,
                block.x + block.w + 3,
                block.y + block.h + 3
            );

            SelectObject(hdc, oldBrush);
            SelectObject(hdc, oldPen);

            DeleteObject(pen);
        }
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

    // 手前のブロックから順に判定
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

    // ブロックがない場所をクリックしたら選択解除
    selected = -1;
    dragging = false;
    dragStarted = false;
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
        // ドラッグ開始前の状態を1回だけ保存
        SaveUndoState();

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

    // selected は解除しない
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
    SaveUndoState();

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

    SaveUndoState();

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

const std::vector<int>& BlockManager::GetParents() const
{
    return parent;
}

bool BlockManager::ReplaceBlocks(
    const std::vector<Block>& newBlocks,
    const std::vector<int>& newParents)
{
    if (newBlocks.size() != newParents.size())
        return false;

    const int count = static_cast<int>(newBlocks.size());

    // 接続先の範囲を検証
    for (int i = 0; i < count; ++i)
    {
        const int p = newParents[i];

        if (p < -1 || p >= count || p == i)
            return false;
    }

    // 接続の循環を検証
    for (int i = 0; i < count; ++i)
    {
        int current = i;

        for (int steps = 0; current != -1; ++steps)
        {
            if (steps >= count)
                return false;

            current = newParents[current];
        }
    }

    blocks = newBlocks;
    parent = newParents;

    undoStack.clear();
    redoStack.clear();

    selected = -1;
    dragging = false;
    dragStarted = false;

    return true;
}

bool BlockManager::IsDragging() const
{
    return dragging;
}

int BlockManager::GetSelectedBlockIndex() const
{
    return selected;
}


int BlockManager::GetSelectedBlock() const
{
    return selected;
}

void BlockManager::ClearSelection()
{
    selected = -1;
    dragging = false;
    dragStarted = false;
}

bool BlockManager::DeleteSelectedBlock()
{
    if (selected < 0 ||
        selected >= static_cast<int>(blocks.size()))
    {
        return false;
    }

    int index = selected;

    SaveUndoState();

    for (int i = 0;
        i < static_cast<int>(parent.size());
        ++i)
    {
        if (parent[i] == index)
            parent[i] = -1;
        else if (parent[i] > index)
            --parent[i];
    }

    blocks.erase(blocks.begin() + index);
    parent.erase(parent.begin() + index);

    ClearSelection();
    return true;
}

bool BlockManager::CopySelectedBlock()
{
    if (selected < 0 ||
        selected >= static_cast<int>(blocks.size()))
    {
        return false;
    }

    copiedBlock = blocks[selected];
    hasCopiedBlock = true;

    return true;
}

bool BlockManager::PasteCopiedBlock(int x, int y)
{
    if (!hasCopiedBlock)
        return false;

    Block newBlock = copiedBlock;

    newBlock.x = x;
    newBlock.y = y;

    SaveUndoState();

    blocks.push_back(newBlock);
    parent.push_back(-1);

    selected = static_cast<int>(blocks.size()) - 1;

    return true;
}

// --------------------------------------------------------
// 操作前の状態を履歴に保存
// --------------------------------------------------------
void BlockManager::SaveUndoState()
{
    BlockState state;
    state.blocks = blocks;
    state.parent = parent;

    undoStack.push_back(state);

    // 新しい操作を行ったら、やり直し履歴を消す
    redoStack.clear();

    // 履歴は最大100件
    constexpr size_t MAX_HISTORY = 100;

    if (undoStack.size() > MAX_HISTORY)
    {
        undoStack.erase(undoStack.begin());
    }
}

// --------------------------------------------------------
// 元に戻す：Ctrl + Z
// --------------------------------------------------------
bool BlockManager::Undo()
{
    if (undoStack.empty())
        return false;

    // 現在の状態をやり直し履歴へ保存
    BlockState current;
    current.blocks = blocks;
    current.parent = parent;

    redoStack.push_back(current);

    // 直前の状態を復元
    BlockState previous = undoStack.back();
    undoStack.pop_back();

    blocks = previous.blocks;
    parent = previous.parent;

    // 古い選択インデックスを使わない
    ClearSelection();

    return true;
}

// --------------------------------------------------------
// やり直す：Ctrl + Y
// --------------------------------------------------------
bool BlockManager::Redo()
{
    if (redoStack.empty())
        return false;

    // 現在の状態を元に戻す履歴へ保存
    BlockState current;
    current.blocks = blocks;
    current.parent = parent;

    undoStack.push_back(current);

    // やり直し状態を復元
    BlockState next = redoStack.back();
    redoStack.pop_back();

    blocks = next.blocks;
    parent = next.parent;

    ClearSelection();

    return true;
}