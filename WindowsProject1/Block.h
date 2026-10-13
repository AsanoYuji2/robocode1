#pragma once
#include <windows.h>
#include <string>
enum class BlockType { Move, Turn, Fire };
struct Block { BlockType type; int x, y; int w=180; int h=54; };
COLORREF GetBlockColor(BlockType type);
std::wstring GetBlockName(BlockType type);
bool HitBlock(const Block& b, int x, int y);
void DrawBlock(HDC hdc, const Block& b);
