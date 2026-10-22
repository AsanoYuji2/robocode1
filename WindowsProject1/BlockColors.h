
#pragma once

#include <windows.h>
#include "Block.h"

// ============================================================
// ブロックカテゴリの色
// ============================================================

inline COLORREF GetBlockCategoryColor(BlockType type)
{
    switch (type)
    {
        // ----------------------------------------------------
        // 移動：青
        // ----------------------------------------------------
    case BlockType::Move:
    case BlockType::MoveBackward:
    case BlockType::MoveRight:
    case BlockType::MoveLeft:
    case BlockType::GetAwayFromWall:
    case BlockType::ZigzagMove:
        return RGB(65, 150, 230);

        // ----------------------------------------------------
        // 旋回・照準：グレー
        // ----------------------------------------------------
    case BlockType::TurnRight:
    case BlockType::TurnLeft:
    case BlockType::TurnGunRight:
    case BlockType::TurnGunLeft:
    case BlockType::TurnRadarRight:
    case BlockType::TurnRadarLeft:
    case BlockType::TurnBodyToEnemy:
    case BlockType::TurnGunToEnemy:
    case BlockType::ForwardBackwardGun360:
        return RGB(120, 120, 130);

        // ----------------------------------------------------
        // 攻撃：オレンジ
        // ----------------------------------------------------
    case BlockType::FireBullet:
    case BlockType::FireBulletAndGetResult:
    case BlockType::KeepAwayRadarFire:
    case BlockType::BodyBlowFire:
    case BlockType::SmartFire:
    case BlockType::HesitateNextBullet:
    case BlockType::Scan:
        return RGB(218, 133, 39);

        // ----------------------------------------------------
        // 制御：黄橙
        // ----------------------------------------------------
    case BlockType::Repeat:
    case BlockType::EndRepeat:
    case BlockType::Wait:
    case BlockType::If:
    case BlockType::Else:
    case BlockType::EndIf:
    case BlockType::DoNothing:
        return RGB(235, 165, 45);

        // ----------------------------------------------------
        // 変数：ピンク
        // ----------------------------------------------------
    case BlockType::CreateVariable:
    case BlockType::SetVariable:
    case BlockType::ChangeVariable:
    case BlockType::AddVariable:
    case BlockType::SubtractVariable:
    case BlockType::SetBoolean:
        return RGB(190, 70, 120);

        // ----------------------------------------------------
        // 情報取得：緑
        // ----------------------------------------------------
    case BlockType::GetName:
    case BlockType::GetX:
    case BlockType::GetY:
    case BlockType::GetEnergy:
    case BlockType::GetHeading:
    case BlockType::GetGunHeading:
    case BlockType::GetRadarHeading:
    case BlockType::GetGunCoolingRate:
    case BlockType::GetGunHeat:
    case BlockType::GetVelocity:
    case BlockType::GetOthers:
    case BlockType::GetBattleFieldHeight:
    case BlockType::GetBattleFieldWidth:
    case BlockType::GetHeight:
    case BlockType::GetWidth:
    case BlockType::GetNumRounds:
    case BlockType::GetRoundNum:
    case BlockType::GetTime:
        return RGB(60, 150, 90);

        // ----------------------------------------------------
        // システム・設定：紫
        // ----------------------------------------------------
    case BlockType::Stop:
    case BlockType::Resume:
    case BlockType::SetAdjustGunForRobotTurn:
    case BlockType::SetAdjustRadarForRobotTurn:
    case BlockType::SetAdjustRadarForGunTurn:
        return RGB(130, 100, 200);

        // ----------------------------------------------------
        // 色・演出：赤
        // ----------------------------------------------------
    case BlockType::SetBodyColor:
    case BlockType::SetGunColor:
    case BlockType::SetRadarColor:
    case BlockType::SetBulletColor:
    case BlockType::SetScanColor:
        return RGB(200, 90, 90);

    case BlockType::VictoryDance:
    case BlockType::VictorySpin:
        return RGB(200, 170, 40);

    default:
        return RGB(100, 100, 100);
    }
}
//他のファイルを1つのファイルを分割して送るので全て送るまで待って