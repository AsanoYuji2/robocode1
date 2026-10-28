#include "BlockManager.h"

#include <cstdlib> 
#include <cmath> 
#include <algorithm> 
#include <vector>
#include "BlockColors.h"

namespace
{
    constexpr int SNAP_DISTANCE = 35;
    constexpr int COLUMN_DISTANCE = 50;
    constexpr int TAB_HEIGHT = 8;
    constexpr int SELECTION_PADDING = 6;
    constexpr int MAX_HISTORY = 100;

    // Block.cpp draws these blocks wider than their default Block::w.
    // Use the same effective width when calculating a range selection.
    int GetRangeSelectionWidth(const Block& block)
    {
        switch (block.type)
        {
        case BlockType::SmartFire:
        case BlockType::ForwardBackwardGun360:
        case BlockType::SetAdjustGunForRobotTurn:
        case BlockType::SetAdjustRadarForRobotTurn:
        case BlockType::SetAdjustRadarForGunTurn:
        case BlockType::GetBattleFieldHeight:
        case BlockType::GetBattleFieldWidth:
        case BlockType::TurnRadarRight:
        case BlockType::TurnRadarLeft:
            return (std::max)(block.w, 250);
        default:
            return block.w;
        }
    }
}

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
    if (!hdc)
        return;

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

    // ========================================
    // ① 通常のノードを優先してホバー判定
    // ========================================
    for (int i = static_cast<int>(blocks.size()) - 1;
        i >= 0; --i)
    {
        if (blocks[i].type == BlockType::Repeat)
            continue;

        if (HitBlock(blocks[i], world.x, world.y))
        {
            hoveredIndex = i;
            break;
        }
    }

    // ========================================
    // ② 通常のノードに当たらなければ
    //    Repeat ブロックを判定
    // ========================================
    if (hoveredIndex == -1)
    {
        for (int i = static_cast<int>(blocks.size()) - 1;
            i >= 0; --i)
        {
            if (blocks[i].type != BlockType::Repeat)
                continue;

            if (HitBlock(blocks[i], world.x, world.y))
            {
                hoveredIndex = i;
                break;
            }
        }
    }

    // ========================================
    // ③ Repeat ブロックを先に描画
    //    ホバー中なら光らせる
    // ========================================
    for (int i = 0; i < static_cast<int>(blocks.size()); ++i)
    {
        if (blocks[i].type != BlockType::Repeat)
            continue;

        const bool isSelected =
            i == selected ||
            std::find(selectedBlocks.begin(), selectedBlocks.end(), i) != selectedBlocks.end();
        bool glowing = (i == hoveredIndex) || isSelected;

        if (dragging &&
            std::find(
                dragBlocks.begin(),
                dragBlocks.end(),
                i) != dragBlocks.end())
        {
            glowing = true;
        }

        DrawBlock(hdc, blocks[i], glowing);
    }

    // ========================================
    // ④ 通常のノードを後から描画
    // ========================================
    for (int i = 0; i < static_cast<int>(blocks.size()); ++i)
    {
        if (blocks[i].type == BlockType::Repeat)
            continue;

        const bool isSelected =
            i == selected ||
            std::find(selectedBlocks.begin(), selectedBlocks.end(), i) != selectedBlocks.end();
        bool glowing = (i == hoveredIndex) || isSelected;

        if (dragging &&
            std::find(
                dragBlocks.begin(),
                dragBlocks.end(),
                i) != dragBlocks.end())
        {
            glowing = true;
        }

        DrawBlock(hdc, blocks[i], glowing);
    }

    // ========================================
    // ⑤ 範囲選択中の四角形
    // ========================================
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
            HGDIOBJ previousPen =
                SelectObject(hdc, rangePen);

            HGDIOBJ previousBrush =
                SelectObject(
                    hdc,
                    GetStockObject(HOLLOW_BRUSH));

            Rectangle(
                hdc,
                left,
                top,
                right,
                bottom);

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
    mouseX = x;
    mouseY = y;
    POINT world = ScreenToWorld(x, y);

    if (deleteMode)
    {
        DeleteBlockAt(world.x, world.y);
        return;
    }

    int hitIndex = -1;

    // ① 通常ブロックを優先して判定する
    for (int i = static_cast<int>(blocks.size()) - 1;
        i >= 0; --i)
    {
        if (blocks[i].type == BlockType::Repeat)
            continue;

        if (HitBlock(blocks[i], world.x, world.y))
        {
            hitIndex = i;
            break;
        }
    }

    // ② 通常ブロックが見つからなかった場合だけ、
    //    Repeat ブロックを判定する
    if (hitIndex == -1)
    {
        for (int i = static_cast<int>(blocks.size()) - 1;
            i >= 0; --i)
        {
            if (blocks[i].type != BlockType::Repeat)
                continue;

            if (HitBlock(blocks[i], world.x, world.y))
            {
                hitIndex = i;
                break;
            }
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

        // dragBlocks は実際に一緒に動かすブロック一覧です。
        // 範囲選択のときは選択範囲、通常のドラッグでは選択ブロックと
        // その下につながるブロックが入っています。
        std::vector<bool> moving(blocks.size(), false);
        for (int index : dragBlocks)
        {
            if (index >= 0 && index < static_cast<int>(blocks.size()))
                moving[static_cast<size_t>(index)] = true;
        }
        if (selected >= 0 && selected < static_cast<int>(blocks.size()))
            moving[static_cast<size_t>(selected)] = true;

        // 1) 動かす側の親が範囲外なら、その接続を切る。
        // 範囲内同士の親子関係はそのまま残す。
        for (int i = 0; i < static_cast<int>(blocks.size()); ++i)
        {
            if (!moving[static_cast<size_t>(i)])
                continue;

            const int p = parent[static_cast<size_t>(i)];
            if (p >= 0 && p < static_cast<int>(blocks.size()) &&
                !moving[static_cast<size_t>(p)])
            {
                parent[static_cast<size_t>(i)] = -1;
            }
        }

        // 2) 動かさない側が、動かす側を親にしている場合も切る。
        // これで選択グループを元の場所から完全に切り離せる。
        for (int i = 0; i < static_cast<int>(blocks.size()); ++i)
        {
            if (moving[static_cast<size_t>(i)])
                continue;

            const int p = parent[static_cast<size_t>(i)];
            if (p >= 0 && p < static_cast<int>(blocks.size()) &&
                moving[static_cast<size_t>(p)])
            {
                parent[static_cast<size_t>(i)] = -1;
            }
        }

        // 3) Repeat ブロックと内側の先頭ブロックの接続も、
        // 片方だけを移動するときは解除する。両方を動かすときは維持。
        for (int i = 0; i < static_cast<int>(blocks.size()); ++i)
        {
            if (blocks[static_cast<size_t>(i)].type != BlockType::Repeat ||
                i >= static_cast<int>(repeatBody.size()))
                continue;

            const int body = repeatBody[static_cast<size_t>(i)];
            if (body < 0 || body >= static_cast<int>(blocks.size()))
                continue;

            const bool repeatMoves = moving[static_cast<size_t>(i)];
            const bool bodyMoves = moving[static_cast<size_t>(body)];
            if (repeatMoves != bodyMoves)
                repeatBody[static_cast<size_t>(i)] = -1;
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

    // ========================================
    // ドラッグ中の Repeat の高さを調整
    // ========================================
    const int count = static_cast<int>(blocks.size());

    // まず、内側のブロックに必要な高さに戻す
    for (int i = 0; i < count; ++i)
    {
        if (blocks[i].type != BlockType::Repeat)
            continue;

        int body = repeatBody[i];
        int lastBody = body;

        for (int step = 0;
            lastBody >= 0 &&
            lastBody < count &&
            step < count;
            ++step)
        {
            int child = GetChild(lastBody);

            if (child < 0)
                break;

            lastBody = child;
        }

        if (body < 0)
        {
            blocks[i].h = 120;
        }
        else
        {
            blocks[i].h = (std::max)(
                120,
                blocks[lastBody].y
                + blocks[lastBody].h
                - blocks[i].y + 30);
        }
    }

    // 1個の通常ブロックをドラッグしているとき、
    // Repeat の一番下に近づいたら、先にスペースを広げる
    if (dragBlocks.size() == 1 &&
        selected >= 0 &&
        selected < count &&
        blocks[selected].type != BlockType::Repeat)
    {
        for (int i = 0; i < count; ++i)
        {
            if (blocks[i].type != BlockType::Repeat)
                continue;

            int body = repeatBody[i];
            int lastBody = body;

            for (int step = 0;
                lastBody >= 0 &&
                lastBody < count &&
                step < count;
                ++step)
            {
                int child = GetChild(lastBody);

                if (child < 0)
                    break;

                lastBody = child;
            }

            const int targetX = blocks[i].x + 24;

            const int targetY =
                (body < 0)
                ? blocks[i].y + 37
                : blocks[lastBody].y
                + blocks[lastBody].h - TAB_HEIGHT;

            const int distanceX =
                std::abs(blocks[selected].x - targetX);

            const int distanceY =
                std::abs(blocks[selected].y - targetY);

            if (distanceX <= SNAP_DISTANCE &&
                distanceY <= SNAP_DISTANCE)
            {
                const int requiredHeight =
                    targetY + blocks[selected].h
                    - blocks[i].y + 30;

                blocks[i].h = (std::max)(
                    blocks[i].h,
                    requiredHeight);
            }
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

    // ========================================
    // ① 移動グループを収集
    // ========================================
    std::vector<int> movingBlocks;
    std::vector<bool> isMoving(static_cast<size_t>(count), false);

    if (selectedBlocks.size() > 1)
    {
        // 範囲選択中は selected (先頭1個) だけではなく、
        // selectedBlocks 全体を移動グループとして扱う。
        for (int index : selectedBlocks)
        {
            if (index >= 0 && index < count && !isMoving[static_cast<size_t>(index)])
            {
                isMoving[static_cast<size_t>(index)] = true;
                movingBlocks.push_back(index);
            }
        }

        if (movingBlocks.empty())
            return;

        // 親がグループ外にないブロックが、このグループの先頭候補。
        // 接続していない複数のグループを選んでいる場合は、誤った接続を
        // 作らないようにスナップ接続を行わない。
        int root = -1;
        int rootCount = 0;
        for (int index : movingBlocks)
        {
            const int p = parent[static_cast<size_t>(index)];
            if (p < 0 || p >= count || !isMoving[static_cast<size_t>(p)])
            {
                root = index;
                ++rootCount;
            }
        }
        if (rootCount != 1)
            return;

        std::vector<int> orderedChain;
        std::vector<bool> visited(static_cast<size_t>(count), false);
        int current = root;
        while (current >= 0 && current < count &&
            isMoving[static_cast<size_t>(current)] &&
            !visited[static_cast<size_t>(current)])
        {
            visited[static_cast<size_t>(current)] = true;
            orderedChain.push_back(current);
            const int child = GetChild(current);
            if (child < 0 || child >= count || !isMoving[static_cast<size_t>(child)])
                break;
            current = child;
        }

        // 範囲内に独立したブロックが混ざっているなら、全体を1つの
        // 接続グループとみなさず、位置をそのまま保つ。
        if (orderedChain.size() != movingBlocks.size())
            return;
        movingBlocks.swap(orderedChain);
    }
    else
    {
        int current = selected;
        for (int step = 0;
            current >= 0 && current < count && step < count;
            ++step)
        {
            if (isMoving[static_cast<size_t>(current)])
                break;

            movingBlocks.push_back(current);
            isMoving[static_cast<size_t>(current)] = true;
            current = GetChild(current);
        }
    }

    if (movingBlocks.empty())
        return;

    // Repeat の内部ブロックも移動対象として登録する。
  // movingBlocks の順序は外側の接続順のまま維持する。
    for (int i = 0; i < count; ++i)
    {
        if (!isMoving[static_cast<size_t>(i)] ||
            blocks[static_cast<size_t>(i)].type != BlockType::Repeat ||
            i >= static_cast<int>(repeatBody.size()))
        {
            continue;
        }

        int body = repeatBody[static_cast<size_t>(i)];

        for (int step = 0;
            body >= 0 && body < count && step < count;
            ++step)
        {
            if (isMoving[static_cast<size_t>(body)])
                break;

            isMoving[static_cast<size_t>(body)] = true;
            body = GetChild(body);
        }
    }


    const int firstBlock = movingBlocks.front();
    const int lastBlock = movingBlocks.back();

    // ========================================
    // ② Repeat の内側への接続
    // ========================================
    if (blocks[firstBlock].type != BlockType::Repeat)
    {
        for (int i = 0; i < count; ++i)
        {
            if (isMoving[i])
                continue;

            if (blocks[i].type != BlockType::Repeat)
                continue;

            if (i >= static_cast<int>(repeatBody.size()))
                continue;

            int firstBody = repeatBody[i];
            int lastBody = firstBody;

            for (int step = 0;
                lastBody >= 0 &&
                lastBody < count &&
                step < count;
                ++step)
            {
                int child = GetChild(lastBody);

                if (child < 0 || isMoving[child])
                    break;

                lastBody = child;
            }

            const int targetX = blocks[i].x + 24;

            const int targetY =
                (firstBody < 0)
                ? blocks[i].y + 46
                : blocks[lastBody].y
                + blocks[lastBody].h - TAB_HEIGHT;

            const int dx =
                std::abs(blocks[firstBlock].x - targetX);

            const int dy =
                std::abs(blocks[firstBlock].y - targetY);

            if (dx > SNAP_DISTANCE || dy > SNAP_DISTANCE)
                continue;

            const int moveX = targetX - blocks[firstBlock].x;
            const int moveY = targetY - blocks[firstBlock].y;

            for (int index = 0; index < count; ++index)
            {
                if (!isMoving[static_cast<size_t>(index)])
                    continue;

                blocks[static_cast<size_t>(index)].x += moveX;
                blocks[static_cast<size_t>(index)].y += moveY;
            }

            if (firstBody < 0)
            {
                repeatBody[i] = firstBlock;
                parent[firstBlock] = -1;
            }
            else
            {
                parent[firstBlock] = lastBody;
            }

            const int bodyBottom =
                blocks[lastBlock].y + blocks[lastBlock].h;

            blocks[i].h = (std::max)(
                120,
                bodyBottom - blocks[i].y + 30);

            return;
        }
    }

    // ========================================
    // ③ 通常ブロックへの接続候補を探す
    // ========================================
    int bestTarget = -1;
    int bestDistance = SNAP_DISTANCE + 1;
    int bestSide = -1;

    // ========================================
// ③-A-1 ブロック同士が重なっている場合
//     接続先の下に挿入する候補として優先する
// ========================================
    for (int i = 0; i < count; ++i)
    {
        if (isMoving[static_cast<size_t>(i)])
            continue;

        const Block& target = blocks[static_cast<size_t>(i)];
        const Block& first = blocks[static_cast<size_t>(firstBlock)];

        const int targetWidth = GetRangeSelectionWidth(target);
        const int firstWidth = GetRangeSelectionWidth(first);

        const int targetHeight = (std::max)(
            target.h,
            target.type == BlockType::Repeat ? 120 : target.h);

        const int firstHeight = (std::max)(
            first.h,
            first.type == BlockType::Repeat ? 120 : first.h);

        // CとAの矩形が重なっているか判定する
        const bool overlaps =
            first.x < target.x + targetWidth &&
            first.x + firstWidth > target.x &&
            first.y < target.y + targetHeight &&
            first.y + firstHeight > target.y;

        if (!overlaps)
            continue;

        // Aに既存の子Bがあっても、Aの下へCを挿入する。
        // 通常の接続候補より優先するため、距離を0にする。
        bestTarget = i;
        bestDistance = 0;
        bestSide = 0;
        break;
    }

    // 0 = 移動グループを接続先の下に挿入
    // 1 = 接続先を移動グループの下につなぐ
    for (int i = 0; i < count; ++i)
    {
        if (isMoving[i])
            continue;

        // 重なりによる挿入候補が見つかっている場合、
        // それより優先度の低い通常候補は調べない。
        if (bestDistance == 0)
            break;

        const Block& target = blocks[i];

        // ----------------------------------------
        // ③-A 接続先の下に移動グループを接続
        // 子ブロックがある場合も候補にする
        // ----------------------------------------
        if (std::abs(blocks[firstBlock].x - target.x)
            < COLUMN_DISTANCE)
        {
            int oldChild = GetChild(i);

            // 移動グループ自身を接続先にしない
            if (oldChild < 0 || !isMoving[oldChild])
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
                    bestSide = 0;
                }
            }
        }

        // ----------------------------------------
        // ③-B 移動グループの下に接続先をつなぐ
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
                bestSide = 1;
            }
        }
    }

    if (bestTarget < 0)
        return;

    // ========================================
    // ④ 接続先の下にグループを挿入
    // ========================================
    // ========================================
// ④ 接続先の下にグループを挿入
// ========================================
    if (bestSide == 0)
    {
        const int target = bestTarget;

        // 接続先に元からつながっているブロック
        const int oldChild = GetChild(target);

        // 接続先AのX座標に完全に合わせる
        const int targetX = blocks[target].x;

        // Aの下端に接続する位置
        const int targetY =
            blocks[target].y + blocks[target].h - TAB_HEIGHT;

        const int moveX =
            targetX - blocks[firstBlock].x;

        const int moveY =
            targetY - blocks[firstBlock].y;

        // CおよびCにつながるブロックをまとめて移動
        for (int index = 0; index < count; ++index)
        {
            if (!isMoving[static_cast<size_t>(index)])
                continue;

            blocks[static_cast<size_t>(index)].x += moveX;
            blocks[static_cast<size_t>(index)].y += moveY;
        }

        // 既存の子Bがあれば、Cの下に接続できる位置へ移動
        if (oldChild >= 0 && !isMoving[oldChild])
        {
            const int newChildX = blocks[lastBlock].x;
            const int newChildY =
                blocks[lastBlock].y
                + blocks[lastBlock].h
                - TAB_HEIGHT;

            const int shiftX =
                newChildX - blocks[oldChild].x;

            const int shiftY =
                newChildY - blocks[oldChild].y;

            int child = oldChild;

            for (int step = 0;
                child >= 0 &&
                child < count &&
                step < count;
                ++step)
            {
                if (isMoving[child])
                    break;

                blocks[child].x += shiftX;
                blocks[child].y += shiftY;

                child = GetChild(child);
            }

            // Cの下にBを接続
            parent[oldChild] = lastBlock;
        }

        // Aの下にCを接続
        parent[firstBlock] = target;

        return;
    }

    // ========================================
    // ⑤ 移動グループの下に接続先をつなぐ
    // ========================================
    const int target = bestTarget;

    const int targetOldParent = parent[target];

    const int moveX =
        blocks[target].x - blocks[lastBlock].x;

    const int moveY =
        blocks[target].y
        - blocks[lastBlock].h + TAB_HEIGHT
        - blocks[lastBlock].y;

    for (int index = 0; index < count; ++index)
    {
        if (!isMoving[static_cast<size_t>(index)])
            continue;

        blocks[static_cast<size_t>(index)].x += moveX;
        blocks[static_cast<size_t>(index)].y += moveY;
    }

    // 移動グループ → 接続先
    parent[target] = lastBlock;

    // 接続先の元の親があれば、その親の下に
    // 移動グループを挿入する
    if (targetOldParent >= 0 &&
        targetOldParent < count &&
        !isMoving[targetOldParent])
    {
        parent[firstBlock] = targetOldParent;
    }
    else
    {
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

void BlockManager::AddBlock(BlockType type, int x, int y)
{
    Block block{};
    block.type = type;
    block.x = x;
    block.y = y;

    // Repeatだけ高さを確保する
    if (type == BlockType::Repeat)
        block.h = 120;

    AddBlock(block);
}

// 設定値を含むブロックを追加する。
// 追加前の状態を1回だけUndo履歴へ保存するため、
// 関数呼び出しブロックなども1回の操作で取り消せる。
void BlockManager::AddBlock(const Block& source)
{
    SaveUndoState();

    Block block = source;
    if (block.type == BlockType::Repeat && block.h < 120)
        block.h = 120;

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

bool BlockManager::ReplaceBlocks(
    const std::vector<Block>& newBlocks,
    const std::vector<int>& newParents)
{
    // 旧形式の呼び出しでは、Repeat内側の接続は未接続として扱う。
    return ReplaceBlocks(
        newBlocks,
        newParents,
        std::vector<int>(newBlocks.size(), -1));
}

bool BlockManager::ReplaceBlocks(
    const std::vector<Block>& newBlocks,
    const std::vector<int>& newParents,
    const std::vector<int>& newRepeatBodies)
{
    if (newBlocks.size() != newParents.size() ||
        newBlocks.size() != newRepeatBodies.size())
        return false;

    const int count = static_cast<int>(newBlocks.size());
    std::vector<bool> hasChild(count, false);

    for (int i = 0; i < count; ++i)
    {
        const int p = newParents[static_cast<size_t>(i)];
        if (p < -1 || p >= count || p == i)
            return false;

        if (p != -1)
        {
            if (hasChild[static_cast<size_t>(p)])
                return false;
            hasChild[static_cast<size_t>(p)] = true;
        }

        const int body = newRepeatBodies[static_cast<size_t>(i)];
        if (body < -1 || body >= count || body == i)
            return false;
        if (body != -1 && newBlocks[static_cast<size_t>(i)].type != BlockType::Repeat)
            return false;
    }

    // 親子のつながりに循環がないか確認する。
    for (int i = 0; i < count; ++i)
    {
        int current = i;
        for (int steps = 0; current != -1; ++steps)
        {
            if (steps >= count)
                return false;
            current = newParents[static_cast<size_t>(current)];
        }
    }

    blocks = newBlocks;
    parent = newParents;
    repeatBody = newRepeatBodies;

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
    std::vector<int> newRepeatBodies;

    std::vector<int> indexMap(blocks.size(), -1);

    for (int i = 0;
        i < static_cast<int>(blocks.size());
        ++i)
    {
        if (!remove[static_cast<size_t>(i)])
        {
            indexMap[static_cast<size_t>(i)] = static_cast<int>(newBlocks.size());
            newBlocks.push_back(blocks[static_cast<size_t>(i)]);
            newParents.push_back(parent[static_cast<size_t>(i)]);
            newRepeatBodies.push_back(
                i < static_cast<int>(repeatBody.size())
                ? repeatBody[static_cast<size_t>(i)] : -1);
        }
    }

    for (int i = 0; i < static_cast<int>(newParents.size()); ++i)
    {
        const int oldParent = newParents[static_cast<size_t>(i)];
        if (oldParent < 0 || oldParent >= static_cast<int>(indexMap.size()) ||
            remove[static_cast<size_t>(oldParent)])
            newParents[static_cast<size_t>(i)] = -1;
        else
            newParents[static_cast<size_t>(i)] = indexMap[static_cast<size_t>(oldParent)];

        const int oldBody = newRepeatBodies[static_cast<size_t>(i)];
        if (oldBody < 0 || oldBody >= static_cast<int>(indexMap.size()) ||
            remove[static_cast<size_t>(oldBody)])
            newRepeatBodies[static_cast<size_t>(i)] = -1;
        else
            newRepeatBodies[static_cast<size_t>(i)] = indexMap[static_cast<size_t>(oldBody)];
    }

    blocks = newBlocks;
    parent = newParents;
    repeatBody = newRepeatBodies;

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

    const int left = (std::min)(selectionStart.x, selectionEnd.x);
    const int top = (std::min)(selectionStart.y, selectionEnd.y);
    // +1 makes a very thin drag still a valid selection rectangle.
    const int right = (std::max)(selectionStart.x, selectionEnd.x) + 1;
    const int bottom = (std::max)(selectionStart.y, selectionEnd.y) + 1;

    selectedBlocks.clear();

    // Drawn width differs from Block::w for some wide blocks, so use the
    // same effective width that the renderer uses. Collect ALL intersections.
    for (int i = 0; i < static_cast<int>(blocks.size()); ++i)
    {
        const Block& block = blocks[static_cast<size_t>(i)];
        const int width = GetRangeSelectionWidth(block);
        const int height = (std::max)(block.h, block.type == BlockType::Repeat ? 120 : block.h);

        const bool intersects =
            block.x < right &&
            block.x + width > left &&
            block.y < bottom &&
            block.y + height > top;

        if (intersects)
            selectedBlocks.push_back(i);
    }

    // Stable index order makes keyboard operations, dragging and deletion
    // operate on the same complete selection each time.
    std::sort(selectedBlocks.begin(), selectedBlocks.end());
    selectedBlocks.erase(
        std::unique(selectedBlocks.begin(), selectedBlocks.end()),
        selectedBlocks.end());

    selected = selectedBlocks.empty() ? -1 : selectedBlocks.front();
}

void BlockManager::EndRangeSelection() { selectingRange = false; }

const std::vector<int>& BlockManager::GetSelectedBlocks() const { return selectedBlocks; }

bool BlockManager::IsSelectingRange() const { return selectingRange; }

const std::vector<int>& BlockManager::GetRepeatBodies() const
{
    return repeatBody;
}

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
    state.repeatBody = repeatBody;

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
    current.repeatBody = repeatBody;

    redoStack.push_back(current);

    BlockState previous = undoStack.back();
    undoStack.pop_back();

    blocks = previous.blocks;
    parent = previous.parent;
    repeatBody = previous.repeatBody;

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
    current.repeatBody = repeatBody;

    PushUndoState(current);

    BlockState next = redoStack.back();
    redoStack.pop_back();

    blocks = next.blocks;
    parent = next.parent;
    repeatBody = next.repeatBody;

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