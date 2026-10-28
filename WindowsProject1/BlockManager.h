#pragma once

#include <windows.h>
#include <vector>
#include "Block.h"

class BlockManager {
public:
    // 初期化・描画
    void Initialize();
    void DrawAll(HDC hdc) const;

    // マウス操作
    void OnMouseDown(HWND hwnd, int x, int y);
    void OnMouseMove(int x, int y);
    void OnMouseUp(HWND hwnd);
    int GetDraggedBlockIndex() const;

    // ブロック操作
    void AddBlock(BlockType type, int x, int y);
    // 設定値を含むブロックを追加する（自作関数の呼び出しブロックにも使用）
    void AddBlock(const Block& block);
    bool DeleteBlockAt(int x, int y);
    const std::vector<Block>& GetBlocks() const;
    bool UpdateBlockAt(int index, const Block& updatedBlock);
    const std::vector<int>& GetParents() const;
    std::vector<int> GetExecutionOrder() const;

    // 削除モード
    void SetDeleteMode(bool enabled);
    bool IsDeleteMode() const;
    bool DeleteBlockAndChildren(int index);

    // 表示位置・ズーム
    void SetViewTransform(float zoom, float offsetX, float offsetY);
    POINT ScreenToWorld(int x, int y) const;
    POINT WorldToScreen(int x, int y) const;

    // ファイル読み込み
    bool ReplaceBlocks(const std::vector<Block>& newBlocks,
        const std::vector<int>& newParents);
    bool ReplaceBlocks(const std::vector<Block>& newBlocks,
        const std::vector<int>& newParents,
        const std::vector<int>& newRepeatBodies);

    // 選択操作
    bool IsDragging() const;
    int GetSelectedBlockIndex() const;
    int GetSelectedBlock() const;
    void SelectBlock(int index);
    void SelectAllBlocks();
    void ClearSelection();

    // コピー・貼り付け・削除
    bool CopySelectedBlock();
    bool PasteCopiedBlock(int x, int y);
    bool DeleteSelectedBlock();
    bool DeleteSelectedBlocks();
    void DeleteBlocks(const std::vector<int>& indices);

    // Undo / Redo
    bool Undo();
    bool Redo();

    // 範囲選択
    void StartRangeSelection(int x, int y);
    void UpdateRangeSelection(int x, int y);
    void EndRangeSelection();
    const std::vector<int>& GetSelectedBlocks() const;
    bool IsSelectingRange() const;
    const std::vector<int>& GetRepeatBodies() const;

private:
    std::vector<int> dragBlocks;
    std::vector<Block> blocks;
    std::vector<int> parent;
    int selected = -1;
    std::vector<int> selectedBlocks;
    int offsetX = 0;
    int offsetY = 0;
    int lastX = 0;
    int lastY = 0;
    int mouseX = -1;
    int mouseY = -1;
    bool dragging = false;
    bool dragStarted = false;
    bool deleteMode = false;
    float zoom = 1.0f;
    float viewX = 350.0f;
    float viewY = 0.0f;
    bool selectingRange = false;
    POINT selectionStart = { 0, 0 };
    POINT selectionEnd = { 0, 0 };
    std::vector<Block> copiedBlocks;
    std::vector<int> copiedParents;
    std::vector<int> copiedBodies;
    bool hasCopiedBlock = false;

    struct BlockState
    {
        std::vector<Block> blocks;
        std::vector<int> parent;
        std::vector<int> repeatBody;
    };

    std::vector<BlockState> undoStack;
    std::vector<BlockState> redoStack;

    bool IsDescendant(int blockIndex, int ancestorIndex) const;
    bool HasChild(int blockIndex) const;
    int GetChild(int blockIndex) const;
    void MoveConnectedBlocks(int dx, int dy);
    void SnapSelectedBlock();
    void PushUndoState(const BlockState& state);
    void SaveUndoState();
    std::vector<int> repeatBody;
};
