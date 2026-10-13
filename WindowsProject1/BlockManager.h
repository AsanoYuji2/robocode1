#pragma once
#include <windows.h>
#include <vector>
#include "Block.h"
class BlockManager {
public:
 void Initialize();
 void DrawAll(HDC hdc) const;
 void OnMouseDown(HWND hwnd,int x,int y);
 void OnMouseMove(int x,int y);
 void OnMouseUp(HWND hwnd);
 const std::vector<Block>& GetBlocks() const;
private:
 std::vector<Block> blocks;
 int selected=-1, offsetX=0, offsetY=0;
 bool dragging=false;
 void SnapSelectedBlock();
};
