#pragma once
#include <windows.h>
#include <string>
enum class BlockType 
{
	Move,        // 前進する
	Turn,	     // 右に旋回
	TurnLeft,    // 左に旋回
	MoveRight,   // 右に移動
	MoveLeft,    // 左に移動
	Fire,        // 弾を発射 威力
	Bark         // 後退
};

struct Block 
{
	BlockType type; 
	int x, y; 
	int w=180; 
	int h=54; 
};

COLORREF GetBlockColor(BlockType type);
std::wstring GetBlockName(BlockType type);
bool HitBlock(const Block& b, int x, int y);
void DrawBlock(HDC hdc, const Block& b);
