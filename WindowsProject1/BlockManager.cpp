#include "BlockManager.h"

#include <cstdlib> 
#include <cmath> 
#include <algorithm> 
#include <vector>
#include "BlockColors.h"

namespace { constexpr int SNAP_DISTANCE = 24; constexpr int COLUMN_DISTANCE = 35; constexpr int TAB_HEIGHT = 8; constexpr int SELECTION_PADDING = 6; constexpr int MAX_HISTORY = 100; }

// ============================================================
// 初期化
// ============================================================

void BlockManager::Initialize() {
    blocks.clear();
    parent.clear();
    repeatBody.clear();

    undoStack.clear();
    redoStack.clear();

    copiedBlocks.clear();
    copiedParents.clear();
    hasCopiedBlock = false;

    // 初期ブロック
    blocks.push_back({ BlockType::Move, 100, 120 });
    blocks.push_back({ BlockType::TurnRight, 100, 220 });
    blocks.push_back({ BlockType::FireBullet, 100, 320 });

    parent.assign(blocks.size(), -1);

    repeatBody.assign(blocks.size(), -1);
    selected = -1;
    selectedBlocks.clear();

    dragging = false;
    dragStarted = false;
    deleteMode = false;
    selectingRange = false;

    selectionStart = { 0, 0 };
    selectionEnd = { 0, 0 };

    zoom = 1.0f;
    viewX = 350.0f;
    viewY = 0.0f;

    mouseX = -1;
    mouseY = -1;
}

// ============================================================ 
// 削除モード 
// ============================================================

void BlockManager::SetDeleteMode(bool enabled) {
    deleteMode = enabled;

    dragging = false;
    dragStarted = false;
    selectingRange = false;

    if (enabled)
    {
        selected = -1;
        selectedBlocks.clear();
    }


}

bool BlockManager::IsDeleteMode() const { return deleteMode; }

// ============================================================ 
//  表示ズーム
//  ============================================================

void BlockManager::SetViewTransform(float newZoom, float newOffsetX, float newOffsetY) {
    zoom = (std::max)(0.25f, (std::min)(newZoom, 3.0f));

    viewX = newOffsetX;
    viewY = newOffsetY;


}

POINT BlockManager::ScreenToWorld(int x, int y) const {
    POINT result{};

    result.x = static_cast<LONG>(
        std::lround((x - viewX) / zoom));

    result.y = static_cast<LONG>(
        std::lround((y - viewY) / zoom));

    return result;


}

POINT BlockManager::WorldToScreen(int x, int y) const {
    POINT result{};

    result.x = static_cast<LONG>(
        std::lround(x * zoom + viewX));

    result.y = static_cast<LONG>(
        std::lround(y * zoom + viewY));

    return result;


}

// ============================================================ 
// 描画
// ============================================================


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

    POINT world = ScreenToWorld(mouseX, mouseY);

    int hoveredIndex = -1;

    // カーソルが乗っているブロックを調べる
    // 後ろから調べることで、手前のブロックを優先する
    for (int i = static_cast<int>(blocks.size()) - 1;
        i >= 0; --i)
    {
        if (HitBlock(blocks[i], world.x, world.y))
        {
            hoveredIndex = i;
            break;
        }
    }

    // 1. Repeatブロックを先に描画する
    for (int i = 0; i < (int)blocks.size(); ++i)
    {
        if (blocks[i].type == BlockType::Repeat)
        {
            DrawBlock(hdc, blocks[i], false);
        }
    }

    // 2. 普通のブロックを後から描画する
    // 後から描画したブロックが手前に表示される
    for (int i = 0; i < (int)blocks.size(); ++i)
    {
        if (blocks[i].type != BlockType::Repeat)
        {
            bool glowing = (i == hoveredIndex);

            if (dragging &&
                std::find(dragBlocks.begin(), dragBlocks.end(), i)
                != dragBlocks.end())
            {
                glowing = true;
            }

            DrawBlock(hdc, blocks[i], glowing);
        }
    }


    // 範囲選択中の四角形
    if (selectingRange)
    {
        int left = (std::min)(
            selectionStart.x, selectionEnd.x);
        int top = (std::min)(
            selectionStart.y, selectionEnd.y);
        int right = (std::max)(
            selectionStart.x, selectionEnd.x);
        int bottom = (std::max)(
            selectionStart.y, selectionEnd.y);

        HPEN rangePen = CreatePen(
            PS_SOLID, 2, RGB(40, 150, 255));

        if (rangePen)
        {
            HGDIOBJ previousPen = SelectObject(hdc, rangePen);
            HGDIOBJ previousBrush = SelectObject(
                hdc, GetStockObject(HOLLOW_BRUSH));

            Rectangle(hdc, left, top, right, bottom);

            SelectObject(hdc, previousBrush);
            SelectObject(hdc, previousPen);
            DeleteObject(rangePen);
        }
    }


    RestoreDC(hdc, savedDC);
}


// ============================================================ 
// マウス操作 
// ============================================================


void BlockManager::OnMouseDown(HWND hwnd, int x, int y)
{
    POINT world = ScreenToWorld(x, y);

    if (deleteMode)
    {
        DeleteBlockAt(world.x, world.y);
        return;
    }

    int hitIndex = -1;

    for (int i = static_cast<int>(blocks.size()) - 1;
        i >= 0; --i)
    {
        if (HitBlock(blocks[i], world.x, world.y))
        {
            hitIndex = i;
            break;
        }
    }

    if (hitIndex < 0)
    {
        // 空白部分からドラッグしたら範囲選択
        selected = -1;
        selectedBlocks.clear();
        dragBlocks.clear();

        dragging = false;
        dragStarted = false;

        // 画面座標ではなくワールド座標で範囲を記録
        StartRangeSelection(world.x, world.y);

        SetCapture(hwnd);
        return;
    }

    // クリックしたブロックを選択
    bool clickedSelectedBlock =
        std::find(
            selectedBlocks.begin(),
            selectedBlocks.end(),
            hitIndex) != selectedBlocks.end();

    if (!clickedSelectedBlock)
    {
        selectedBlocks.clear();
    }

    selected = hitIndex;

    offsetX = world.x - blocks[hitIndex].x;
    offsetY = world.y - blocks[hitIndex].y;

    lastX = world.x;
    lastY = world.y;

    dragBlocks.clear();

    if (!selectedBlocks.empty())
    {
        dragBlocks = selectedBlocks;
    }
    else
    {
        int current = selected;

        std::vector<bool> added(blocks.size(), false);

        for (int count = 0;
            current >= 0 &&
            current < static_cast<int>(blocks.size()) &&
            count < static_cast<int>(blocks.size());
            ++count)
        {
            if (added[current])
                break;

            added[current] = true;
            dragBlocks.push_back(current);

            // Repeat の内側ブロックも移動対象にする
            if (blocks[current].type == BlockType::Repeat &&
                current < static_cast<int>(repeatBody.size()))
            {
                int body = repeatBody[current];

                for (int bodyCount = 0;
                    body >= 0 &&
                    body < static_cast<int>(blocks.size()) &&
                    bodyCount < static_cast<int>(blocks.size());
                    ++bodyCount)
                {
                    if (added[body])
                        break;

                    added[body] = true;
                    dragBlocks.push_back(body);

                    body = GetChild(body);
                }
            }

            current = GetChild(current);
        }
    }

    std::sort(dragBlocks.begin(), dragBlocks.end());

    dragBlocks.erase(
        std::unique(dragBlocks.begin(), dragBlocks.end()),
        dragBlocks.end());

    std::sort(dragBlocks.begin(), dragBlocks.end());

    dragBlocks.erase(
        std::unique(dragBlocks.begin(), dragBlocks.end()),
        dragBlocks.end());

    dragging = true;
    dragStarted = false;

    SetCapture(hwnd);
}

void BlockManager::OnMouseMove(int x, int y)
{
    mouseX = x;
    mouseY = y;

    // 範囲選択中なら、矩形を更新する
    if (selectingRange)
    {
        POINT world = ScreenToWorld(x, y);

        UpdateRangeSelection(world.x, world.y);
        return;
    }

    if (!dragging ||
        selected < 0 ||
        selected >= static_cast<int>(blocks.size()))
    {
        return;
    }

    POINT world = ScreenToWorld(x, y);

    int dx = world.x - lastX;
    int dy = world.y - lastY;

    if (dx == 0 && dy == 0)
        return;

    if (!dragStarted)
    {
        SaveUndoState();

        if (selectedBlocks.empty())
        {
            parent[selected] = -1;
        }

        dragStarted = true;
    }

    for (int index : dragBlocks)
    {
        if (index >= 0 &&
            index < static_cast<int>(blocks.size()))
        {
            blocks[index].x += dx;
            blocks[index].y += dy;
        }
    }

    lastX = world.x;
    lastY = world.y;
}

void BlockManager::OnMouseUp(HWND hwnd)
{
    if (selectingRange)
    {
        POINT world = ScreenToWorld(mouseX, mouseY);

        UpdateRangeSelection(world.x, world.y);
        EndRangeSelection();
    }
    else if (dragging && selected >= 0 && dragStarted)
    {
        SnapSelectedBlock();
    }

    dragging = false;
    dragStarted = false;

    if (GetCapture() == hwnd)
        ReleaseCapture();
}

// ============================================================ 
// 接続されたブロックの移動
// ============================================================

void BlockManager::MoveConnectedBlocks(int dx, int dy) { for (int i = 0; i < static_cast<int>(blocks.size()); ++i) { if (i == selected || IsDescendant(i, selected)) { blocks[i].x += dx; blocks[i].y += dy; } } }

bool BlockManager::IsDescendant(int blockIndex, int ancestorIndex) const {
    if (blockIndex < 0 || ancestorIndex < 0 || blockIndex >= static_cast<int>(parent.size())) { return false; }

    int current = parent[blockIndex];

    for (int count = 0;
        current >= 0 &&
        count < static_cast<int>(blocks.size());
        ++count)
    {
        if (current == ancestorIndex)
            return true;

        if (current >= static_cast<int>(parent.size()))
            return false;

        current = parent[current];
    }

    return false;


}

bool BlockManager::HasChild(int blockIndex) const 
{
   
    // 外側の接続だけを調べる。
        // Repeat の内側は repeatBody で別管理する。
        for (int p : parent)
        {
            if (p == blockIndex)
                return true;
        }

    return false;

}

int BlockManager::GetChild(int blockIndex) const 
{
    // 外側・通常の接続を取得する
    for (int i = 0; i < static_cast<int>(parent.size()); ++i)
    {
        if (parent[i] == blockIndex)
            return i;
    }

    return -1;

}

// ============================================================ 
// ブロックの接続 
// ============================================================
void BlockManager::SnapSelectedBlock()
{
    const int count = static_cast<int>(blocks.size());

    if (selected < 0 || selected >= count)
        return;

    // 選択ブロックと、その下の子孫を収集する
    std::vector<int> movingBlocks;
    int current = selected;

    for (int step = 0;
        current >= 0 && current < count && step < count;
        ++step)
    {
        movingBlocks.push_back(current);
        current = GetChild(current);
    }

    if (movingBlocks.empty())
        return;

    const int firstBlock = movingBlocks.front();
    const int lastBlock = movingBlocks.back();

    std::vector<bool> isMoving(count, false);

    for (int index : movingBlocks)
        isMoving[index] = true;

    // ========================================
   // ① Repeat の内側に接続する
   // ========================================
    if (movingBlocks.size() == 1)
    {
        const int movingIndex = movingBlocks.front();

        if (blocks[movingIndex].type != BlockType::Repeat)
        {
            for (int i = 0; i < count; ++i)
            {
                if (i == movingIndex || isMoving[i])
                    continue;

                if (blocks[i].type != BlockType::Repeat)
                    continue;

                // Repeat の内側の接続位置
                const int expectedX = blocks[i].x + 48;
                const int expectedY = blocks[i].y + 35;

                const int dx =
                    std::abs(blocks[movingIndex].x - expectedX);

                const int dy =
                    std::abs(blocks[movingIndex].y - expectedY);

                if (dx > SNAP_DISTANCE || dy > SNAP_DISTANCE)
                    continue;

                // 内側の接続先が既にある場合は使わない
                if (i >= static_cast<int>(repeatBody.size()))
                    continue;

                if (repeatBody[i] != -1)
                    continue;

                if (IsDescendant(i, movingIndex))
                    continue;

                // Repeat の内側に配置
                blocks[movingIndex].x = expectedX - 24;
                blocks[movingIndex].y = expectedY + 2;

                // 内側の接続は repeatBody だけに保存する
                repeatBody[i] = movingIndex;

                // 外側の接続関係とは分離する
                parent[movingIndex] = -1;

                return;
            }
        }
    }
    

    // ========================================
    // ② 通常の上下接続を探す
    // ========================================
    int bestTarget = -1;
    int bestDistance = SNAP_DISTANCE + 1;
    int bestY = 0;
    int bestSide = -1;

    for (int i = 0; i < count; ++i)
    {
        if (isMoving[i])
            continue;

        const Block& target = blocks[i];

        // ----------------------------------------
        // ②-1 移動グループを相手の下側に接続
        // ----------------------------------------
        if (!HasChild(i) &&
            std::abs(blocks[firstBlock].x - target.x)
            < COLUMN_DISTANCE)
        {
            const int expectedY =
                target.y + target.h - TAB_HEIGHT;

            const int distance =
                std::abs(blocks[firstBlock].y - expectedY);

            if (distance <= SNAP_DISTANCE &&
                distance < bestDistance)
            {
                bestTarget = i;
                bestDistance = distance;
                bestY = expectedY;
                bestSide = 0;
            }
        }

        // ----------------------------------------
        // ②-2 相手を移動グループの下側に接続
        // ----------------------------------------
        if (std::abs(blocks[lastBlock].x - target.x)
            < COLUMN_DISTANCE)
        {
            const int expectedY =
                target.y - blocks[lastBlock].h + TAB_HEIGHT;

            const int distance =
                std::abs(blocks[lastBlock].y - expectedY);

            if (distance <= SNAP_DISTANCE &&
                distance < bestDistance)
            {
                bestTarget = i;
                bestDistance = distance;
                bestY = expectedY;
                bestSide = 1;
            }
        }
    }

    if (bestTarget < 0)
        return;

    // ========================================
    // ③ 接続位置にグループ全体を移動
    // ========================================
    const int attachBlock =
        (bestSide == 0) ? firstBlock : lastBlock;

    const int dx =
        blocks[bestTarget].x - blocks[attachBlock].x;

    const int dy =
        bestY - blocks[attachBlock].y;

    for (int index : movingBlocks)
    {
        blocks[index].x += dx;
        blocks[index].y += dy;
    }

    // ========================================
    // ④ 接続関係を更新
    // ========================================
    if (bestSide == 0)
    {
        // 相手 → 移動グループ
        parent[firstBlock] = bestTarget;
    }
    else
    {
        // 移動グループ → 相手
        const int oldParent = parent[bestTarget];

        parent[bestTarget] = lastBlock;

        if (oldParent >= 0 && !isMoving[oldParent])
            parent[firstBlock] = oldParent;
        else
            parent[firstBlock] = -1;
    }
}

// ============================================================ 
// 実行順序
// ============================================================

std::vector<int> BlockManager::GetExecutionOrder() const {
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

// ============================================================ 
// ブロック一覧
// ============================================================

const std::vector<Block>& BlockManager::GetBlocks() const { return blocks; }

bool BlockManager::UpdateBlockAt(int index, const Block& updatedBlock)
{
    if (index < 0 || index >= static_cast<int>(blocks.size()))
        return false;
    SaveUndoState();
    blocks[static_cast<size_t>(index)] = updatedBlock;
    return true;
}

const std::vector<int>& BlockManager::GetParents() const { return parent; }

bool BlockManager::IsDragging() const { return dragging; }

int BlockManager::GetSelectedBlockIndex() const { return selected; }

int BlockManager::GetSelectedBlock() const { return selected; }

// ============================================================ // ブロック追加 // ============================================================

void BlockManager::AddBlock(BlockType type, int x, int y) {
    SaveUndoState();

    
    Block block{};
    block.type = type;
    block.x = x;
    block.y = y;

    // Repeatだけ高さを確保する
    if (type == BlockType::Repeat)
    {
        block.h = 120;
    }

    blocks.push_back(block);
    parent.push_back(-1);
    repeatBody.push_back(-1);

}

// ============================================================ 
// 指定位置のブロックを削除 
// ============================================================

bool BlockManager::DeleteBlockAt(int x, int y) {
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

    std::vector<int> targets{ index };

    // 削除したブロックの子孫も削除対象にする
    for (size_t i = 0; i < targets.size(); ++i)
    {
        int current = targets[i];

        for (int j = 0;
            j < static_cast<int>(parent.size());
            ++j)
        {
            if (parent[j] == current)
                targets.push_back(j);
        }
    }

    DeleteBlocks(targets);
    return true;


}

bool BlockManager::DeleteBlockAndChildren(int index)
{
    if (index < 0 ||
        index >= static_cast<int>(blocks.size()))
    {
        return false;
    }

    std::vector<int> targets{ index };

    // 接続されている子孫ブロックをすべて収集
    for (size_t i = 0; i < targets.size(); ++i)
    {
        int current = targets[i];

        for (int j = 0;
            j < static_cast<int>(parent.size());
            ++j)
        {
            if (parent[j] == current)
            {
                // 重複登録を防止
                if (std::find(
                    targets.begin(),
                    targets.end(),
                    j) == targets.end())
                {
                    targets.push_back(j);
                }
            }
        }
    }

    // 既存の削除処理を利用
    DeleteBlocks(targets);

    return true;
}

// ============================================================ 
// ブロックの置き換え
// ============================================================

bool BlockManager::ReplaceBlocks(const std::vector<Block>& newBlocks, const std::vector<int>& newParents) {
    if (newBlocks.size() != newParents.size()) return false;

    const int count = static_cast<int>(newBlocks.size());

    std::vector<bool> hasChild(count, false);

    for (int i = 0; i < count; ++i)
    {
        int p = newParents[i];

        if (p < -1 || p >= count || p == i)
            return false;

        if (p != -1)
        {
            if (hasChild[p])
                return false;

            hasChild[p] = true;
        }
    }

    for (int i = 0; i < count; ++i)
    {
        int current = i;

        for (int steps = 0;
            current != -1;
            ++steps)
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
    selectedBlocks.clear();

    dragging = false;
    dragStarted = false;
    selectingRange = false;

    return true;


}

// ============================================================ 
// 選択解除 
// ============================================================

void BlockManager::ClearSelection() {
    selected = -1;
    selectedBlocks.clear();

    dragging = false;
    dragStarted = false;


}

void BlockManager::SelectBlock(int index)
{
    if (index < 0 ||
        index >= static_cast<int>(blocks.size()))
    {
        ClearSelection();
        return;
    }

    selected = index;
    selectedBlocks.clear();

    dragging = false;
    dragStarted = false;
}

// ============================================================ 
// 択したブロックを削除 
// ============================================================

bool BlockManager::DeleteSelectedBlock() {
    if (!selectedBlocks.empty()) return DeleteSelectedBlocks();

    if (selected < 0 ||
        selected >= static_cast<int>(blocks.size()))
    {
        return false;
    }

    DeleteBlocks({ selected });
    return true;


}

// ============================================================ 
// 複数ブロックをまとめて削除 
// ============================================================

bool BlockManager::DeleteSelectedBlocks() {
    if (selectedBlocks.empty()) return false;

    DeleteBlocks(selectedBlocks);
    return true;


}

// ============================================================ 
// 指定された複数ブロックを削除 
// ============================================================

void BlockManager::DeleteBlocks(const std::vector<int>& indices) {
    if (indices.empty()) return;

    std::vector<bool> remove(
        blocks.size(),
        false);

    for (int index : indices)
    {
        if (index >= 0 &&
            index < static_cast<int>(blocks.size()))
        {
            remove[index] = true;
        }
    }

    bool any = false;

    for (bool value : remove)
    {
        if (value)
        {
            any = true;
            break;
        }
    }

    if (!any)
        return;

    SaveUndoState();

    std::vector<Block> newBlocks;
    std::vector<int> newParents;

    std::vector<int> indexMap(blocks.size(), -1);

    for (int i = 0;
        i < static_cast<int>(blocks.size());
        ++i)
    {
        if (!remove[i])
        {
            indexMap[i] = static_cast<int>(newBlocks.size());
            newBlocks.push_back(blocks[i]);
            newParents.push_back(parent[i]);
        }
    }

    for (int i = 0;
        i < static_cast<int>(newParents.size());
        ++i)
    {
        int oldParent = newParents[i];

        if (oldParent < 0 ||
            oldParent >= static_cast<int>(indexMap.size()))
        {
            newParents[i] = -1;
        }
        else if (remove[oldParent])
        {
            newParents[i] = -1;
        }
        else
        {
            newParents[i] = indexMap[oldParent];
        }
    }

    blocks = newBlocks;
    parent = newParents;

    ClearSelection();


}

// ============================================================ 
// 複数選択 
// ============================================================

void BlockManager::StartRangeSelection(int x, int y) {
    selectingRange = true;

    selectionStart = { x, y };
    selectionEnd = { x, y };

    selected = -1;
    selectedBlocks.clear();

    dragging = false;
    dragStarted = false;


}

void BlockManager::UpdateRangeSelection(int x, int y) {
    if (!selectingRange) return;

    selectionEnd = { x, y };

    int left = (std::min)(selectionStart.x, selectionEnd.x);
    int top = (std::min)(selectionStart.y, selectionEnd.y);
    int right = (std::max)(selectionStart.x, selectionEnd.x);
    int bottom = (std::max)(selectionStart.y, selectionEnd.y);

    selectedBlocks.clear();

    // 矩形に交差するブロックをすべて選択
    for (int i = 0;
        i < static_cast<int>(blocks.size());
        ++i)
    {
        const Block& block = blocks[i];

        bool intersects =
            block.x < right &&
            block.x + block.w > left &&
            block.y < bottom &&
            block.y + block.h > top;

        if (intersects)
            selectedBlocks.push_back(i);
    }

    selected = selectedBlocks.empty()
        ? -1
        : selectedBlocks.front();


}

void BlockManager::EndRangeSelection() { selectingRange = false; }

const std::vector<int>& BlockManager::GetSelectedBlocks() const { return selectedBlocks; }

bool BlockManager::IsSelectingRange() const { return selectingRange; }

// ============================================================ 
// コピー
// ============================================================

bool BlockManager::CopySelectedBlock() {
    std::vector<int> selection;

    if (!selectedBlocks.empty())
    {
        selection = selectedBlocks;
    }
    else if (selected >= 0 &&
        selected < static_cast<int>(blocks.size()))
    {
        selection.push_back(selected);

        for (size_t i = 0; i < selection.size(); ++i)
        {
            int current = selection[i];
            int child = GetChild(current);

            if (child >= 0)
                selection.push_back(child);
        }
    }
    else
    {
        return false;
    }

    std::sort(selection.begin(), selection.end());
    selection.erase(
        std::unique(selection.begin(), selection.end()),
        selection.end());

    copiedBlocks.clear();
    copiedParents.clear();

    std::vector<int> indexMap(
        blocks.size(),
        -1);

    for (int index : selection)
    {
        if (index < 0 ||
            index >= static_cast<int>(blocks.size()))
        {
            continue;
        }

        indexMap[index] =
            static_cast<int>(copiedBlocks.size());

        copiedBlocks.push_back(blocks[index]);
        copiedParents.push_back(-1);
    }

    // �R�s�[���ɗ����̃u���b�N���܂܂��ꍇ�����ڑ����ێ�
    for (size_t i = 0; i < selection.size(); ++i)
    {
        int oldIndex = selection[i];

        if (oldIndex < 0 ||
            oldIndex >= static_cast<int>(parent.size()))
        {
            continue;
        }

        int oldParent = parent[oldIndex];

        if (oldParent >= 0 &&
            oldParent < static_cast<int>(indexMap.size()) &&
            indexMap[oldParent] >= 0)
        {
            copiedParents[i] = indexMap[oldParent];
        }
    }

    hasCopiedBlock = !copiedBlocks.empty();

    return hasCopiedBlock;


}

// ============================================================ 
// 貼り付け
// ============================================================

bool BlockManager::PasteCopiedBlock(int x, int y) {
    if (!hasCopiedBlock || copiedBlocks.empty()) return false;

    int dx = x - copiedBlocks[0].x;
    int dy = y - copiedBlocks[0].y;

    SaveUndoState();

    int startIndex = static_cast<int>(blocks.size());

    selectedBlocks.clear();

    for (size_t i = 0; i < copiedBlocks.size(); ++i)
    {
        Block newBlock = copiedBlocks[i];

        newBlock.x += dx;
        newBlock.y += dy;

        blocks.push_back(newBlock);

        int copiedParent = copiedParents[i];

        if (copiedParent >= 0)
            parent.push_back(startIndex + copiedParent);
        else
            parent.push_back(-1);

        selectedBlocks.push_back(
            static_cast<int>(blocks.size()) - 1);
    }

    selected = selectedBlocks.empty()
        ? -1
        : selectedBlocks.front();

    return true;


}

// ============================================================ 
// Undo 
// ============================================================

void BlockManager::PushUndoState(const BlockState& state) {
    undoStack.push_back(state);

    if (undoStack.size() > MAX_HISTORY)
        undoStack.erase(undoStack.begin());


}

void BlockManager::SaveUndoState() {
    BlockState state;

    state.blocks = blocks;
    state.parent = parent;

    PushUndoState(state);

    redoStack.clear();


}

// ============================================================ 
// 元に戻す
// ============================================================

bool BlockManager::Undo() {
    if (undoStack.empty()) return false;

    BlockState current;

    current.blocks = blocks;
    current.parent = parent;

    redoStack.push_back(current);

    BlockState previous = undoStack.back();
    undoStack.pop_back();

    blocks = previous.blocks;
    parent = previous.parent;

    ClearSelection();

    return true;


}

// ============================================================ 
// やり直す
// ============================================================

bool BlockManager::Redo() {
    if (redoStack.empty()) return false;

    BlockState current;

    current.blocks = blocks;
    current.parent = parent;

    PushUndoState(current);

    BlockState next = redoStack.back();
    redoStack.pop_back();

    blocks = next.blocks;
    parent = next.parent;

    ClearSelection();

    return true;
}


void BlockManager::SelectAllBlocks()
{
    selectedBlocks.clear();

    for (int i = 0; i < static_cast<int>(blocks.size()); ++i)
    {
        selectedBlocks.push_back(i);
    }

    selected = selectedBlocks.empty()
        ? -1
        : selectedBlocks.front();

    dragging = false;
    dragStarted = false;
    dragBlocks.clear();
}

int BlockManager::GetDraggedBlockIndex() const
{
    if (!dragging || selected < 0 ||
        selected >= static_cast<int>(blocks.size()))
    {
        return -1;
    }

    return selected;
}