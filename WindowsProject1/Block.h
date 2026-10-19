#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <functional>
#include <unordered_map>

// ============================================================
// ブロックの種類の共通定義
//
// 新しいブロックを追加するときは、ここに追加する。
// BlockType とリスト表示用の一覧が自動的に生成される。
// ============================================================

#define BLOCK_TYPE_LIST(X) \
    /* 移動系 */    \
    X(Move)         \
    X(MoveRight)    \
    X(MoveLeft)     \
    X(MoveBackward) \
    X(TurnRight)    \
    X(TurnLeft)     \
    X(Stop)         \
    X(Resume)       \
    \
    /* 砲塔・レーダー系 */ \
    X(TurnGunRight) \
    X(TurnGunLeft) \
    X(TurnRadarRight) \
    X(TurnRadarLeft) \
    X(SetAdjustGunForRobotTurn) \
    X(SetAdjustRadarForRobotTurn) \
    X(SetAdjustRadarForGunTurn) \
    \
    /* 攻撃系 */ \
    X(FireBullet) \
    \
    /* 待機・制御系 */ \
    X(DoNothing) \
    X(Wait) \
    \
    /* 繰り返し・条件分岐系 */ \
    X(Repeat) \
    X(EndRepeat) \
    X(If) \
    X(Else) \
    X(EndIf) \
    \
    /* 変数系 */ \
    X(CreateVariable) \
    X(SetVariable) \
    X(ChangeVariable) \
    X(AddVariable) \
    X(SubtractVariable) \
    X(SetBoolean) \
    \
    /* 戦闘ロジック系 */ \
    X(KeepAwayRadarFire) \
    X(BodyBlowFire) \
    X(SmartFire) \
    X(GetAwayFromWall) \
    X(TurnBodyToEnemy) \
    X(TurnGunToEnemy) \
    X(HesitateNextBullet) \
    \
    /* 勝利時の動作 */ \
    X(VictoryDance) \
    X(VictorySpin) \
    \
    /* 色変更系 */ \
    X(SetBodyColor) \
    X(SetGunColor) \
    X(SetRadarColor) \
    X(SetBulletColor) \
    X(SetScanColor) \
    \
    /* ゲーム情報取得系 */ \
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
    \
    /* 特殊行動系 */\
    X(FireBulletAndGetResult)\
    X(Scan)\
    \
    /* 関数系 */\
    X(ForwardBackwardGun360)\
    X(ZigzagMove)\

// ============================================================
// ブロックの種類
// ============================================================

enum class BlockType
{
#define DECLARE_BLOCK_TYPE(name) name,
    BLOCK_TYPE_LIST(DECLARE_BLOCK_TYPE)
#undef DECLARE_BLOCK_TYPE
};

// ============================================================
// 条件の種類
// ============================================================

enum class ConditionType
{
    Always,
    HealthAtLeast,

    // 数値変数の比較
    VariableAtLeast,
    VariableAtMost,
    VariableEqual,
    VariableNotEqual,
    VariableGreaterThan,
    VariableLessThan,

    // 敵センサー
    EnemyDetected,
    EnemyDistanceAtMost,
    EnemyOnLeft,
    EnemyOnRight,
    EnemyAhead,
    EnemyBehind,

    // 壁センサー
    WallNear,

    // エネルギー
    EnergyAtMost,

    // 論理演算
    And,
    Or,
    Not,

    // 真偽値
    TrueValue,
    FalseValue,
    BooleanVariable
};

// ============================================================
// イベントの種類
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
// 変数の種類
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

// ============================================================
// ブロックデータ
// ============================================================

struct Block
{
    BlockType type = BlockType::Move;

    // 描画位置・サイズ
    int x = 0;
    int y = 0;
    int w = 180;
    int h = 54;

    // 移動距離
    int distance = 0;

    // 繰り返し回数
    int repeatCount = 0;

    // 待機時間
    double waitSeconds = 0.;

    // 条件分岐
    ConditionType condition = ConditionType::Always;
    int conditionValue = 50;

    // 数値変数
    std::wstring variableName = L"score";
    int value = 0;

    // 真偽値
    bool booleanValue = true;

    // AND / OR / NOT 用の左側条件
    ConditionType leftCondition = ConditionType::Always;
    std::wstring leftVariableName = L"score";
    int leftConditionValue = 0;

    // AND / OR 用の右側条件
    ConditionType rightCondition = ConditionType::Always;
    std::wstring rightVariableName = L"score";
    int rightConditionValue = 0;

    // 真偽値変数名
    std::wstring booleanVariableName = L"flag";

    // 旋回角度
    double degrees = 0.0;

    // 取得するゲーム情報
    RobotInfoType infoType = RobotInfoType::Energy;

    // 砲弾の威力
    double power = 1.0;

    // 色設定（RGB）
    COLORREF bodyColor = RGB(0, 0, 255);
    COLORREF gunColor = RGB(255, 0, 0);
    COLORREF radarColor = RGB(0, 255, 0);
    COLORREF bulletColor = RGB(255, 255, 0);
    COLORREF scanColor = RGB(255, 255, 255);
};

// ============================================================
// イベントハンドラー
// ============================================================

struct EventHandler
{
    RobotEvent event = RobotEvent::Start;
    std::vector<Block> blocks;
};

// ============================================================
// 変数の管理
// ============================================================

// 数値変数
using RobotVariables =
std::unordered_map<std::wstring, int>;

// 真偽値変数
using RobotBooleanVariables =
std::unordered_map<std::wstring, bool>;

// ============================================================
// ブロック種類の一覧取得
//
// Block.cpp で BLOCK_TYPE_LIST(X) から生成する。
// main.cpp では、この関数を使ってリストを作成する。
// ============================================================

const std::vector<BlockType>& GetAvailableBlockTypes();

// ============================================================
// ブロック関連
// ============================================================

COLORREF GetBlockColor(BlockType type);

std::wstring GetBlockName(const Block& block);

std::wstring GetBlockName(BlockType type);

bool HitBlock(const Block& block, int x, int y);

void DrawBlock(HDC hdc, const Block& block, bool glowing = false);

// ============================================================
// ブロック列の実行
// ============================================================

// 変数を新規作成する実行版
void ExecuteBlockSequence(
    const std::vector<Block>& blocks,
    const std::function<void(const Block&)>& executeOne,
    const std::function<bool(const Block&)>& conditionCheck
);

// 数値変数を共有する実行版
void ExecuteBlockSequence(
    const std::vector<Block>& blocks,
    const std::function<void(const Block&)>& executeOne,
    const std::function<bool(const Block&)>& conditionCheck,
    RobotVariables& variables
);

// 数値変数と真偽値変数を共有する実行版
void ExecuteBlockSequence(
    const std::vector<Block>& blocks,
    const std::function<void(const Block&)>& executeOne,
    const std::function<bool(const Block&)>& conditionCheck,
    RobotVariables& variables,
    RobotBooleanVariables& booleanVariables
);

// ============================================================
// イベント関連
// ============================================================

// イベント名の取得
std::wstring GetEventName(RobotEvent event);

// 変数を新規作成するイベント実行版
void ExecuteEventSequence(
    const EventHandler& handler,
    const std::function<void(const Block&)>& executeOne,
    const std::function<bool(const Block&)>& conditionCheck
);

// 数値変数を共有するイベント実行版
void ExecuteEventSequence(
    const EventHandler& handler,
    const std::function<void(const Block&)>& executeOne,
    const std::function<bool(const Block&)>& conditionCheck,
    RobotVariables& variables
);

// 数値変数と真偽値変数を共有するイベント実行版
void ExecuteEventSequence(
    const EventHandler& handler,
    const std::function<void(const Block&)>& executeOne,
    const std::function<bool(const Block&)>& conditionCheck,
    RobotVariables& variables,
    RobotBooleanVariables& booleanVariables
);

// ============================================================
// イベント管理クラス
// ============================================================

class RobotEventManager
{
public:
    // イベントと変数をすべてクリア
    void Clear();

    // イベントの登録・更新
    void RegisterEvent(
        RobotEvent event,
        const std::vector<Block>& blocks
    );

    // イベント発生時の実行
    void TriggerEvent(
        RobotEvent event,
        const std::function<void(const Block&)>& executeOne,
        const std::function<bool(const Block&)>& conditionCheck
    );

    // 登録済みイベントの取得
    const std::vector<EventHandler>& GetHandlers() const;

    // 数値変数
    RobotVariables& GetVariables();
    const RobotVariables& GetVariables() const;

    // 真偽値変数
    RobotBooleanVariables& GetBooleanVariables();
    const RobotBooleanVariables& GetBooleanVariables() const;

private:
    // 登録済みイベント
    std::vector<EventHandler> handlers;

    // イベント間で共有する変数
    RobotVariables variables;
    RobotBooleanVariables booleanVariables;
};