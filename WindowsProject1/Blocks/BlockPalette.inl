// Categories for the block palette. The function category is reserved for
// callback-name markers and user-defined function calls.
std::vector<BlockType> GetBlockTypes(BlockCategory category);

BlockCategory GetCategoryFromBlockType(BlockType type);

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
            BlockType::GetAwayFromWall,
            BlockType::ZigzagMove
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
            BlockType::OnScannedRobot,
            BlockType::OnHitWall,
            BlockType::OnBulletHit,
            BlockType::OnHitByBullet,
            BlockType::OnHitRobot,
            BlockType::OnWin
        };

    case BlockCategory::Attack:
        return {
            BlockType::FireBullet,
            BlockType::FireBulletAndGetResult,
            BlockType::KeepAwayRadarFire,
            BlockType::BodyBlowFire,
            BlockType::SmartFire,
            BlockType::HesitateNextBullet,
            BlockType::Scan
        };

    case BlockCategory::Event:
        return {
            BlockType::VictoryDance,
            BlockType::VictorySpin
        };

    case BlockCategory::Rotate:
        return {
            BlockType::TurnRight,
            BlockType::TurnLeft,
            BlockType::TurnGunRight,
            BlockType::TurnGunLeft,
            BlockType::TurnRadarRight,
            BlockType::TurnRadarLeft,
            BlockType::TurnBodyToEnemy,
            BlockType::TurnGunToEnemy,
            BlockType::ForwardBackwardGun360
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
    default:
        return {};
    }
}
