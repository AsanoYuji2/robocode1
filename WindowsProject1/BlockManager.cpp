#include "BlockManager.h"
#include <cstdlib>
namespace { constexpr int SNAP_DISTANCE = 24; constexpr int TAB_HEIGHT = 8; }
void BlockManager::Initialize() { blocks.clear(); blocks.push_back({ BlockType::Move,100,120 }); blocks.push_back({ BlockType::Turn,100,220 }); blocks.push_back({ BlockType::Fire,100,320 }); }
void BlockManager::DrawAll(HDC hdc) const { for (const auto& b : blocks) DrawBlock(hdc, b); }
void BlockManager::OnMouseDown(HWND hwnd, int x, int y)
{
	for (int i = (int)blocks.size() - 1;i >= 0;--i) if (HitBlock(blocks[i], x, y)) { selected = i; offsetX = x - blocks[i].x; offsetY = y - blocks[i].y; dragging = true; SetCapture(hwnd); return; }
}
void BlockManager::OnMouseMove(int x, int y) { if (dragging && selected >= 0) { blocks[selected].x = x - offsetX; blocks[selected].y = y - offsetY; } }
void BlockManager::OnMouseUp(HWND hwnd) { if (dragging && selected >= 0) SnapSelectedBlock(); dragging = false; selected = -1; ReleaseCapture(); }
void BlockManager::SnapSelectedBlock()
{
	Block& moving = blocks[selected];
	for (int i = 0;i < (int)blocks.size();++i)
	{
		if (i == selected) continue; Block& target = blocks[i];
		bool sameColumn = abs(moving.x - target.x) < 35; int expectedY = target.y + target.h - TAB_HEIGHT;
		if (sameColumn && abs(moving.y - expectedY) <= SNAP_DISTANCE) { moving.x = target.x; moving.y = expectedY; break; }
	}
}
const std::vector<Block>& BlockManager::GetBlocks() const { return blocks; }
