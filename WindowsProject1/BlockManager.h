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

    int GetDraggedBlockIndex() const;

    // ========================================================
    // ブロック操作
    // ========================================================

    void AddBlock(BlockType type, int x, int y);

    bool DeleteBlockAt(int x, int y);

    const std::vector<Block>& GetBlocks() const;

    bool UpdateBlockAt(int index, const Block& updatedBlock);

    const std::vector<int>& GetParents() const;

    std::vector<int> GetExecutionOrder() const;

    // ========================================================
    // 削除モード
    // ========================================================

    void SetDeleteMode(bool enabled);

    bool IsDeleteMode() const;

    bool DeleteBlockAndChildren(int index);

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
    // 選択操作
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

    // 選択中のブロック、または複数選択したブロックをコピーする
    bool CopySelectedBlock();

    // コピーしたブロックを指定位置に貼り付ける
    bool PasteCopiedBlock(int x, int y);

    // 選択中のブロック、または複数選択したブロックを削除する
    bool DeleteSelectedBlock();

    // 複数選択したブロックをまとめて削除する
    bool DeleteSelectedBlocks();

    // 指定されたインデックスのブロックをまとめて削除する
    void DeleteBlocks(const std::vector<int>& indices);

    // ========================================================
    // Undo / Redo（操作の取り消し・やり直し）
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

    const std::vector<int>& GetRepeatBodies() const;

private:
    // ドラッグ開始時に移動対象となるブロックのインデックスを保存する
    std::vector<int> dragBlocks;

    // 管理しているすべてのブロック
    std::vector<Block> blocks;

    // parent[i] はブロック i の親ブロックのインデックス
    // -1 は親ブロックが存在しないことを表す
    std::vector<int> parent;

    // 単一選択中のブロックのインデックス
    int selected = -1;

    // 範囲選択中のブロックのインデックス一覧
    std::vector<int> selectedBlocks;

    // ドラッグ開始位置からの X 方向のオフセット
    int offsetX = 0;

    // ドラッグ開始位置からの Y 方向のオフセット
    int offsetY = 0;

    // 前回のマウス位置
    int lastX = 0;

    int lastY = 0;

    int mouseX = -1;

    int mouseY = -1;

    // ドラッグ中かどうか
    bool dragging = false;

    // ドラッグが実際に開始されたかどうか
    bool dragStarted = false;

    // 削除モードが有効かどうか
    bool deleteMode = false;

    // ズーム倍率
    float zoom = 1.0f;

    // 表示位置のオフセット
    float viewX = 350.0f;

    float viewY = 0.0f;

    // 範囲選択中かどうか
    bool selectingRange = false;

    // 範囲選択の開始位置と終了位置
    POINT selectionStart = { 0, 0 };

    POINT selectionEnd = { 0, 0 };

    // コピーしたブロック
    std::vector<Block> copiedBlocks;

    // コピーしたブロック間の接続関係を保存する
    // コピー後のブロックのインデックスで管理する
    std::vector<int> copiedParents;

    // コピー済みのブロックが存在するかどうか
    bool hasCopiedBlock = false;

    // Undo / Redo 用のブロック状態
    struct BlockState
    {
        // ブロック一覧
        std::vector<Block> blocks;

        // ブロック間の親子関係
        std::vector<int> parent;

        std::vector<int> repeatBody;
    };

    // 操作を取り消すための状態履歴
    std::vector<BlockState> undoStack;

    // 取り消した操作をやり直すための状態履歴
    std::vector<BlockState> redoStack;

    // 指定したブロックが祖先ブロックの子孫かどうかを判定する
    bool IsDescendant(
        int blockIndex,
        int ancestorIndex
    ) const;

    // 指定したブロックに子ブロックがあるか判定する
    bool HasChild(int blockIndex) const;

    // 指定したブロックの子ブロックのインデックスを取得する
    int GetChild(int blockIndex) const;

    // 接続されているブロックをまとめて移動する
    void MoveConnectedBlocks(int dx, int dy);

    // 選択中のブロックを接続可能な位置に吸着させる
    void SnapSelectedBlock();

    // Undo 履歴に状態を追加する
    void PushUndoState(const BlockState& state);

    // 現在の状態を Undo 履歴に保存する
    void SaveUndoState();

    // Repeatブロックの内側に接続されたブロックの先頭。
    // repeatBody[i] == -1 の場合、内側ブロックなし。
    std::vector<int> repeatBody;
};