#pragma once

#include <windows.h> 
#include <vector>

#include "Block.h"

class BlockManager {
public: 
    // ======================================================== 
    // 初期化・描画 
    // ========================================================

    void Initialize();

    void DrawAll(HDC hdc) const;

    // ========================================================
    // マウス操作
    // ========================================================

    void OnMouseDown(HWND hwnd, int x, int y);

    void OnMouseMove(int x, int y);

    void OnMouseUp(HWND hwnd);

    // ========================================================
    // ブロック操作
    // ========================================================

    void AddBlock(BlockType type, int x, int y);

    bool DeleteBlockAt(int x, int y);

    const std::vector<Block>& GetBlocks() const;

    const std::vector<int>& GetParents() const;

    std::vector<int> GetExecutionOrder() const;

    // ========================================================
    // 削除モード
    // ========================================================

    void SetDeleteMode(bool enabled);

    bool IsDeleteMode() const;

    // ========================================================
    // 表示位置・ズーム
    // ========================================================

    void SetViewTransform(
        float zoom,
        float offsetX,
        float offsetY
    );

    POINT ScreenToWorld(int x, int y) const;

    POINT WorldToScreen(int x, int y) const;

    // ========================================================
    // ファイル読み込み
    // ========================================================

    bool ReplaceBlocks(
        const std::vector<Block>& newBlocks,
        const std::vector<int>& newParents
    );

    // ========================================================
    // 選択状態
    // ========================================================

    bool IsDragging() const;

    int GetSelectedBlockIndex() const;

    int GetSelectedBlock() const;

    void SelectBlock(int index);

    void SelectAllBlocks();

    void ClearSelection();

    // ========================================================
    // コピー・貼り付け・削除
    // ========================================================

    // 選択したブロック、または複数選択したブロックをコピー
    bool CopySelectedBlock();

    // コピーしたブロックを貼り付け
    bool PasteCopiedBlock(int x, int y);

    // 選択したブロック、または複数選択したブロックを削除
    bool DeleteSelectedBlock();

    // 複数ブロックをまとめて削除
    bool DeleteSelectedBlocks();

    // 指定されたブロックをまとめて削除する内部処理
    void DeleteBlocks(const std::vector<int>& indices);

    // ========================================================
    // Undo / Redo
    // ========================================================

    bool Undo();

    bool Redo();

    // ========================================================
    // 範囲選択
    // ========================================================

    void StartRangeSelection(int x, int y);

    void UpdateRangeSelection(int x, int y);

    void EndRangeSelection();

    const std::vector<int>& GetSelectedBlocks() const;

    bool IsSelectingRange() const;


private: 
    // ドラッグ開始時に移動対象を保存
    std::vector<int> dragBlocks;

    std::vector<Block> blocks;

    // parent[i] はブロック i の親ブロックのインデックス
    // -1 は親ブロックなし
    std::vector<int> parent;

    // 単一選択時のブロックインデックス
    int selected = -1;

    // 範囲選択時の複数ブロックのインデックス
    std::vector<int> selectedBlocks;

    int offsetX = 0;

    int offsetY = 0;

    int lastX = 0;

    int lastY = 0;

    bool dragging = false;

    bool dragStarted = false;

    // 削除モード
    bool deleteMode = false;

    float zoom = 1.0f;

    float viewX = 350.0f;

    float viewY = 0.0f;

    bool selectingRange = false;

    POINT selectionStart = { 0, 0 };

    POINT selectionEnd = { 0, 0 };

    std::vector<Block> copiedBlocks;

    // コピーしたブロック同士の接続関係
    // コピー内のインデックスで管理する
    std::vector<int> copiedParents;

    bool hasCopiedBlock = false;

    struct BlockState
    {
        std::vector<Block> blocks;

        std::vector<int> parent;
    };

    std::vector<BlockState> undoStack;

    std::vector<BlockState> redoStack;

    bool IsDescendant(
        int blockIndex,
        int ancestorIndex
    ) const;

    bool HasChild(int blockIndex) const;

    int GetChild(int blockIndex) const;

    void MoveConnectedBlocks(int dx, int dy);

    void SnapSelectedBlock();

    void PushUndoState(const BlockState& state);

    void SaveUndoState();


};