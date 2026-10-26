std::vector<BlockType> GetBlockTypes(BlockCategory category)
{
    switch (category)
    {
    case BlockCategory::Move:
        return {
            BlockType::Move,
            BlockType::MoveBackward,
            BlockType::MoveRight,
            BlockType::MoveLeft,
            BlockType::GetAwayFromWall
        };

    case BlockCategory::Control:
        return {
            BlockType::Repeat,
            BlockType::EndRepeat,
            BlockType::Wait,
            BlockType::If,
            BlockType::Else,
            BlockType::EndIf,
            BlockType::DoNothing
        };

    case BlockCategory::Variables:
        return {
            BlockType::CreateVariable,
            BlockType::SetVariable,
            BlockType::ChangeVariable,
            BlockType::AddVariable,
            BlockType::SubtractVariable,
            BlockType::SetBoolean
        };

    case BlockCategory::Function:
        return {
            BlockType::KeepAwayRadarFire,
            BlockType::BodyBlowFire,
            BlockType::SmartFire,
            BlockType::TurnBodyToEnemy,
            BlockType::TurnGunToEnemy,
            BlockType::HesitateNextBullet,
            BlockType::VictoryDance,
            BlockType::VictorySpin,
            BlockType::ForwardBackwardGun360,
            BlockType::ZigzagMove
        };

    case BlockCategory::Event:
        // イベントは RobotEventManager 側で管理されています。
        // 現状の BlockType にはイベント専用ブロックがありません。
        return {};

    case BlockCategory::Rotate:
        return {
            BlockType::TurnRight,
            BlockType::TurnLeft,
            BlockType::TurnGunRight,
            BlockType::TurnGunLeft,
            BlockType::TurnRadarRight,
            BlockType::TurnRadarLeft
        };

    case BlockCategory::Color:
        return {
            BlockType::SetBodyColor,
            BlockType::SetGunColor,
            BlockType::SetRadarColor,
            BlockType::SetBulletColor,
            BlockType::SetScanColor
        };

    case BlockCategory::Information:
        return {
            BlockType::GetName,
            BlockType::GetX,
            BlockType::GetY,
            BlockType::GetEnergy,
            BlockType::GetHeading,
            BlockType::GetGunHeading,
            BlockType::GetRadarHeading,
            BlockType::GetGunCoolingRate,
            BlockType::GetGunHeat,
            BlockType::GetVelocity,
            BlockType::GetOthers,
            BlockType::GetBattleFieldHeight,
            BlockType::GetBattleFieldWidth,
            BlockType::GetHeight,
            BlockType::GetWidth,
            BlockType::GetNumRounds,
            BlockType::GetRoundNum,
            BlockType::GetTime
        };

    case BlockCategory::System:
        return {
            BlockType::Stop,
            BlockType::Resume,
            BlockType::SetAdjustGunForRobotTurn,
            BlockType::SetAdjustRadarForRobotTurn,
            BlockType::SetAdjustRadarForGunTurn
        };

    case BlockCategory::Other:
        return {
            BlockType::FireBullet,
             BlockType::FireBulletAndGetResult,
            BlockType::Scan
        };

    default:
        return {};
    }
}

// ============================================================
// ブロック一覧の更新
// ============================================================

