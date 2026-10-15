#pragma once

#include <windows.h>
#include <vector>
#include "Block.h"

class BlockManager
{
public:
    void Initialize();

    void DrawAll(HDC hdc) const;

    void OnMouseDown(HWND hwnd, int x, int y);
    void OnMouseMove(int x, int y);
    void OnMouseUp(HWND hwnd);

    void AddBlock(BlockType type, int x, int y);
    bool DeleteBlockAt(int x, int y);

    const std::vector<Block>& GetBlocks() const;
    std::vector<int> GetExecutionOrder() const;

    // 削除モード
    void SetDeleteMode(bool enabled);
    bool IsDeleteMode() const;

    // カメラの設定
    void SetViewTransform(float zoom, float offsetX, float offsetY);

    // 画面座標とワールド座標の変換
    POINT ScreenToWorld(int x, int y) const;
    POINT WorldToScreen(int x, int y) const;

    // 保存・読み込み用
    const std::vector<int>& GetParents() const;

    bool ReplaceBlocks(
        const std::vector<Block>& newBlocks,
        const std::vector<int>& newParents);

    bool IsDragging() const;

    // 選択中のブロック
    int GetSelectedBlockIndex() const;

    // 選択中のブロックを削除
    bool DeleteSelectedBlock();

    // 選択中のブロックを解除
    void ClearSelection();

    // 選択中ブロックの操作
    bool CopySelectedBlock();
    bool PasteCopiedBlock(int x, int y);

    // 選択状態の取得・解除
    int GetSelectedBlock() const;

    // 元に戻す・やり直す
    bool Undo();
    bool Redo();
private:
    std::vector<Block> blocks;
    std::vector<int> parent;

    int selected = -1;
    int offsetX = 0;
    int offsetY = 0;
    int lastX = 0;
    int lastY = 0;

    bool dragging = false;
    bool dragStarted = false;
    bool deleteMode = false;

    float zoom = 1.0f;
    float viewX = 350.0f;
    float viewY = 0.0f;

    void SnapSelectedBlock();
    void MoveConnectedBlocks(int dx, int dy);

    bool IsDescendant(int blockIndex, int ancestorIndex) const;
    bool HasChild(int blockIndex) const;
    int GetChild(int blockIndex) const;

    // コピーしたブロック
    Block copiedBlock;
    bool hasCopiedBlock = false;

    // 操作履歴の1状態
    struct BlockState
    {
        std::vector<Block> blocks;
        std::vector<int> parent;
    };

    // 元に戻す履歴
    std::vector<BlockState> undoStack;

    // やり直す履歴
    std::vector<BlockState> redoStack;

    // 操作前の状態を保存
    void SaveUndoState();
};