#include "Block.h"
COLORREF GetBlockColor(BlockType t)
{
	switch (t) { case BlockType::Move:return RGB(52, 120, 212); case BlockType::Turn:return RGB(38, 153, 100); case BlockType::Fire:return RGB(218, 133, 39); } return RGB(100, 100, 100);
}
std::wstring GetBlockName(BlockType t)
{
	switch (t) { case BlockType::Move:return L"前進する 100"; case BlockType::Turn:return L"右に旋回 90度"; case BlockType::Fire:return L"弾を発射 威力2"; } return L"不明";
}
bool HitBlock(const Block& b, int x, int y) { return x >= b.x && x <= b.x + b.w && y >= b.y && y <= b.y + b.h; }
void DrawBlock(HDC hdc, const Block& b)
{
	const int tabH = 8; HBRUSH brush = CreateSolidBrush(GetBlockColor(b.type)); HBRUSH old = (HBRUSH)SelectObject(hdc, brush);
	Rectangle(hdc, b.x, b.y + tabH, b.x + b.w, b.y + b.h);
	RECT notch = { b.x + 24,b.y,b.x + 64,b.y + tabH + 1 }; FillRect(hdc, &notch, (HBRUSH)GetStockObject(WHITE_BRUSH));
	RECT tab = { b.x + 24,b.y + b.h - tabH,b.x + 64,b.y + b.h + 1 }; FillRect(hdc, &tab, brush);
	SetBkMode(hdc, TRANSPARENT); SetTextColor(hdc, RGB(255, 255, 255));
	RECT tr = { b.x + 12,b.y + 10,b.x + b.w - 8,b.y + b.h - 4 }; auto name = GetBlockName(b.type);
	DrawTextW(hdc, name.c_str(), -1, &tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
	SelectObject(hdc, old); DeleteObject(brush);
}
