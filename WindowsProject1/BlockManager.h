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

    const std::vector<Block>& GetBlocks() const;
    std::vector<int> GetExecutionOrder() const;

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

    void SnapSelectedBlock();
    void MoveConnectedBlocks(int dx, int dy);

    bool IsDescendant(int blockIndex, int ancestorIndex) const;
    bool HasChild(int blockIndex) const;
    int GetChild(int blockIndex) const;
};