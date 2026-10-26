#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <functional>
#include <unordered_map>


#define BLOCK_TYPE_LIST(X) \
    X(Move)         \
    X(MoveRight)    \
    X(MoveLeft)     \
    X(MoveBackward) \
    X(TurnRight)    \
    X(TurnLeft)     \
    X(Stop)         \
    X(Resume)       \
    X(TurnGunRight) \
    X(TurnGunLeft) \
    X(TurnRadarRight) \
    X(TurnRadarLeft) \
    X(SetAdjustGunForRobotTurn) \
    X(SetAdjustRadarForRobotTurn) \
    X(SetAdjustRadarForGunTurn) \
    X(FireBullet) \
    X(DoNothing) \
    X(Wait) \
    X(Repeat) \
    X(EndRepeat) \
    X(If) \
    X(Else) \
    X(EndIf) \
    X(CreateVariable) \
    X(SetVariable) \
    X(ChangeVariable) \
    X(AddVariable) \
    X(SubtractVariable) \
    X(SetBoolean) \
    X(KeepAwayRadarFire) \
    X(BodyBlowFire) \
    X(SmartFire) \
    X(GetAwayFromWall) \
    X(TurnBodyToEnemy) \
    X(TurnGunToEnemy) \
    X(HesitateNextBullet) \
    X(VictoryDance) \
    X(VictorySpin) \
    X(SetBodyColor) \
    X(SetGunColor) \
    X(SetRadarColor) \
    X(SetBulletColor) \
    X(SetScanColor) \
    X(GetName) \
    X(GetX) \
    X(GetY) \
    X(GetEnergy) \
    X(GetHeading) \
    X(GetGunHeading) \
    X(GetRadarHeading) \
    X(GetGunCoolingRate) \
    X(GetGunHeat) \
    X(GetVelocity) \
    X(GetOthers) \
    X(GetBattleFieldHeight) \
    X(GetBattleFieldWidth) \
    X(GetHeight) \
    X(GetWidth) \
    X(GetNumRounds) \
    X(GetRoundNum) \
    X(GetTime)     \
    X(FireBulletAndGetResult)\
    X(Scan)\
    X(ForwardBackwardGun360)\
    X(ZigzagMove)\


enum class BlockType
{
#define DECLARE_BLOCK_TYPE(name) name,
    BLOCK_TYPE_LIST(DECLARE_BLOCK_TYPE)
#undef DECLARE_BLOCK_TYPE
};

enum class ConditionType
{
    Always,
    HealthAtLeast,
    VariableAtLeast,
    VariableAtMost,
    VariableEqual,
    VariableNotEqual,
    VariableGreaterThan,
    VariableLessThan,
    EnemyDetected,
    EnemyDistanceAtMost,
    EnemyOnLeft,
    EnemyOnRight,
    EnemyAhead,
    EnemyBehind,
    WallNear,
    EnergyAtMost,
    And,
    Or,
    Not,
    TrueValue,
    FalseValue,
    BooleanVariable
};

// ============================================================
// ============================================================

enum class RobotEvent
{
    Start,
    EnemyDetected,
    Damaged,
    Hit,
    Collision,
    WallHit,
    BulletHit,
    BulletMissed,
    BulletHitBullet,
    Death,
    HitByBullet,
    HitRobot,
    RobotDeath,
    Win
};

// ============================================================
// ============================================================

enum class VariableType
{
    Number,
    Boolean
};

enum class RobotInfoType
{
    Name,
    X,
    Y,
    Energy,
    Heading,
    GunHeading,
    RadarHeading,
    GunCoolingRate,
    GunHeat,
    Velocity,
    Others,
    BattleFieldHeight,
    BattleFieldWidth,
    Height,
    Width,
    NumRounds,
    RoundNum,
    Time
};


struct Block
{
    BlockType type = BlockType::Move;

    int x = 0;
    int y = 0;
    int w = 180;
    int h = 54;

    int distance = 0;

    int repeatCount = 0;
    double waitSeconds = 0.;

    ConditionType condition = ConditionType::Always;
    int conditionValue = 50;

    std::wstring variableName = L"score";
    int value = 0;

    bool booleanValue = true;

    ConditionType leftCondition = ConditionType::Always;
    std::wstring leftVariableName = L"score";
    int leftConditionValue = 0;

    ConditionType rightCondition = ConditionType::Always;
    std::wstring rightVariableName = L"score";
    int rightConditionValue = 0;

    std::wstring booleanVariableName = L"flag";

    double degrees = 0.0;

    RobotInfoType infoType = RobotInfoType::Energy;

    double power = 1.0;
    COLORREF bodyColor = RGB(0, 0, 255);
    COLORREF gunColor = RGB(255, 0, 0);
    COLORREF radarColor = RGB(0, 255, 0);
    COLORREF bulletColor = RGB(255, 255, 0);
    COLORREF scanColor = RGB(255, 255, 255);
};

// ============================================================
// ============================================================

struct EventHandler
{
    RobotEvent event = RobotEvent::Start;
    std::vector<Block> blocks;
};

// ============================================================
// ============================================================

using RobotVariables =
std::unordered_map<std::wstring, int>;

using RobotBooleanVariables =
std::unordered_map<std::wstring, bool>;


const std::vector<BlockType>& GetAvailableBlockTypes();

// ============================================================
// ============================================================

COLORREF GetBlockColor(BlockType type);

std::wstring GetBlockName(const Block& block);

std::wstring GetBlockName(BlockType type);

bool HitBlock(const Block& block, int x, int y);

void DrawBlock(HDC hdc, const Block& block, bool glowing = false);

// ============================================================
// ============================================================

void ExecuteBlockSequence(
    const std::vector<Block>& blocks,
    const std::function<void(const Block&)>& executeOne,
    const std::function<bool(const Block&)>& conditionCheck
);

void ExecuteBlockSequence(
    const std::vector<Block>& blocks,
    const std::function<void(const Block&)>& executeOne,
    const std::function<bool(const Block&)>& conditionCheck,
    RobotVariables& variables
);

void ExecuteBlockSequence(
    const std::vector<Block>& blocks,
    const std::function<void(const Block&)>& executeOne,
    const std::function<bool(const Block&)>& conditionCheck,
    RobotVariables& variables,
    RobotBooleanVariables& booleanVariables
);

// ============================================================
// ============================================================

std::wstring GetEventName(RobotEvent event);

void ExecuteEventSequence(
    const EventHandler& handler,
    const std::function<void(const Block&)>& executeOne,
    const std::function<bool(const Block&)>& conditionCheck
);

void ExecuteEventSequence(
    const EventHandler& handler,
    const std::function<void(const Block&)>& executeOne,
    const std::function<bool(const Block&)>& conditionCheck,
    RobotVariables& variables
);

void ExecuteEventSequence(
    const EventHandler& handler,
    const std::function<void(const Block&)>& executeOne,
    const std::function<bool(const Block&)>& conditionCheck,
    RobotVariables& variables,
    RobotBooleanVariables& booleanVariables
);

// ============================================================
// 
// ============================================================

class RobotEventManager
{
public:
    void Clear();

    void RegisterEvent(
        RobotEvent event,
        const std::vector<Block>& blocks
    );
    void TriggerEvent(
        RobotEvent event,
        const std::function<void(const Block&)>& executeOne,
        const std::function<bool(const Block&)>& conditionCheck
    );

    const std::vector<EventHandler>& GetHandlers() const;
    RobotVariables& GetVariables();
    const RobotVariables& GetVariables() const;

    RobotBooleanVariables& GetBooleanVariables();
    const RobotBooleanVariables& GetBooleanVariables() const;

private:

    std::vector<EventHandler> handlers;

    RobotVariables variables;
    RobotBooleanVariables booleanVariables;
};