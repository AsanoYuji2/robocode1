#include "Block.h"
#include "BlockColors.h"
#include "UserFunction.h"

#include <algorithm>
#include <vector>
#include <chrono>
#include <thread>
#include <limits>
#include <sstream>
#include <iomanip>

// ============================================================
// ブロック種類の一覧取得
// Block.h の BLOCK_TYPE_LIST(X) から自動生成する
// ============================================================

const std::vector<BlockType>& GetAvailableBlockTypes()
{
    static const std::vector<BlockType> types =
    {
#define ADD_BLOCK(name) BlockType::name,
        BLOCK_TYPE_LIST(ADD_BLOCK)
#undef ADD_BLOCK
    };

    return types;
}

// ============================================================
// ブロックの色
// ============================================================


COLORREF GetBlockColor(BlockType type)
{
    switch (type)
    {
        // Movement
    case BlockType::Move:
    case BlockType::MoveRight:
    case BlockType::MoveLeft:
    case BlockType::MoveBackward:
    case BlockType::GetAwayFromWall:
    case BlockType::ZigzagMove:
        return RGB(65, 150, 230);

        // Rotation and aiming
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

        // Attack and combat helpers
    case BlockType::FireBullet:
    case BlockType::FireBulletAndGetResult:
    case BlockType::KeepAwayRadarFire:
    case BlockType::BodyBlowFire:
    case BlockType::SmartFire:
    case BlockType::HesitateNextBullet:
    case BlockType::Scan:
        return RGB(218, 133, 39);

        // Control
    case BlockType::DoNothing:
    case BlockType::Wait:
    case BlockType::Repeat:
    case BlockType::EndRepeat:
    case BlockType::If:
    case BlockType::Else:
    case BlockType::EndIf:
        return RGB(235, 165, 45);

        // Variables
    case BlockType::CreateVariable:
    case BlockType::SetVariable:
    case BlockType::ChangeVariable:
    case BlockType::AddVariable:
    case BlockType::SubtractVariable:
    case BlockType::SetBoolean:
        return RGB(190, 70, 120);

        // Function palette entries and user-function calls
    case BlockType::OnScannedRobot:
    case BlockType::OnHitWall:
    case BlockType::OnBulletHit:
    case BlockType::OnHitByBullet:
    case BlockType::OnHitRobot:
    case BlockType::OnWin:
        return RGB(220, 90, 150);

        // Victory-related actions
    case BlockType::VictoryDance:
    case BlockType::VictorySpin:
        return RGB(200, 170, 40);

        // Robot colors
    case BlockType::SetBodyColor:
    case BlockType::SetGunColor:
    case BlockType::SetRadarColor:
    case BlockType::SetBulletColor:
    case BlockType::SetScanColor:
        return RGB(200, 90, 90);

        // Information getters
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

        // System configuration
    case BlockType::Stop:
    case BlockType::Resume:
    case BlockType::SetAdjustGunForRobotTurn:
    case BlockType::SetAdjustRadarForRobotTurn:
    case BlockType::SetAdjustRadarForGunTurn:
        return RGB(130, 100, 200);
    }

    return RGB(100, 100, 100);
}


// ============================================================
// ブロックを光らせるための明るい色
// ============================================================

static COLORREF GetBrightBlockColor(BlockType type)
{
    COLORREF color = GetBlockColor(type);

    int r = GetRValue(color);
    int g = GetGValue(color);
    int b = GetBValue(color);

    // 元の色を白に近づけて明るくする
    r = r + (255 - r) * 50 / 100;
    g = g + (255 - g) * 50 / 100;
    b = b + (255 - b) * 50 / 100;

    return RGB(r, g, b);
}

static COLORREF MakeBrighterColor(COLORREF color)
{
    int r = GetRValue(color);
    int g = GetGValue(color);
    int b = GetBValue(color);

    r = r + (255 - r) * 50 / 100;
    g = g + (255 - g) * 50 / 100;
    b = b + (255 - b) * 50 / 100;

    return RGB(r, g, b);
}

// ============================================================
// 条件名
// ============================================================

static std::wstring GetConditionName(
    ConditionType condition,
    const std::wstring& variableName,
    int value,
    const std::wstring& booleanVariableName = L"flag")
{
    switch (condition)
    {
    case ConditionType::Always:
        return L"常に真";

    case ConditionType::HealthAtLeast:
        return L"体力 >= " + std::to_wstring(value);

    case ConditionType::VariableAtLeast:
        return variableName + L" >= " +
            std::to_wstring(value);

    case ConditionType::VariableAtMost:
        return variableName + L" <= " +
            std::to_wstring(value);

    case ConditionType::VariableEqual:
        return variableName + L" == " +
            std::to_wstring(value);

    case ConditionType::VariableNotEqual:
        return variableName + L" != " +
            std::to_wstring(value);

    case ConditionType::VariableGreaterThan:
        return variableName + L" > " +
            std::to_wstring(value);

    case ConditionType::VariableLessThan:
        return variableName + L" < " +
            std::to_wstring(value);

    case ConditionType::EnemyDetected:
        return L"敵を発見";

    case ConditionType::EnemyDistanceAtMost:
        return L"敵との距離 <= " +
            std::to_wstring(value);

    case ConditionType::EnemyOnLeft:
        return L"敵が左側";

    case ConditionType::EnemyOnRight:
        return L"敵が右側";

    case ConditionType::EnemyAhead:
        return L"敵が前方";

    case ConditionType::EnemyBehind:
        return L"敵が後方";

    case ConditionType::WallNear:
        return L"壁との距離 <= " +
            std::to_wstring(value);

    case ConditionType::EnergyAtMost:
        return L"エネルギー <= " +
            std::to_wstring(value);

    case ConditionType::And:
        return L"AND（かつ）";

    case ConditionType::Or:
        return L"OR（または）";

    case ConditionType::Not:
        return L"NOT（反転）";

    case ConditionType::TrueValue:
        return L"真（TRUE）";

    case ConditionType::FalseValue:
        return L"偽（FALSE）";

    case ConditionType::BooleanVariable:
        return booleanVariableName + L" が真";
    }

    return L"不明な条件";
}

// ============================================================
// 数値を見やすい文字列に変換
// ============================================================

static std::wstring FormatNumber(double value)
{
    std::wostringstream stream;

    // 小数点以下を最大2桁まで表示
    stream << std::fixed << std::setprecision(2) << value;

    std::wstring result = stream.str();

    // 小数点以下の末尾の0を削除
    if (result.find(L'.') != std::wstring::npos)
    {
        while (!result.empty() && result.back() == L'0')
            result.pop_back();

        if (!result.empty() && result.back() == L'.')
            result.pop_back();
    }

    return result;
}

// ============================================================
// ブロック名
// ============================================================

std::wstring GetBlockName(const Block& b)
{
    if (IsUserFunctionCall(b))
        return L"関数を実行： " + GetUserFunctionCallName(b);

    switch (b.type)
    {
    case BlockType::Move:
        return L"前進する  " +
            std::to_wstring(b.distance);

    case BlockType::MoveBackward:
        return L"後退する " +
            std::to_wstring(b.distance);

    case BlockType::TurnRight:
        return L"右に旋回 " +
            FormatNumber(b.degrees) + L"     度";

    case BlockType::TurnLeft:
        return L"左に旋回 " +
            FormatNumber(b.degrees) + L"     度";

    case BlockType::MoveRight:
        return L"右に移動  " +
            std::to_wstring(b.distance);

    case BlockType::MoveLeft:
        return L"左に移動  " +
            std::to_wstring(b.distance);

    case BlockType::FireBullet:
        return L"弾を発射 威力 " +
            FormatNumber(b.power);

    case BlockType::Repeat:
        return L"繰り返す  " +
            std::to_wstring(b.repeatCount) + L" 回";

    case BlockType::EndRepeat:
        return L"繰り返しここまで";

    case BlockType::Wait:
        return L"指定秒数待つ " +
            FormatNumber(b.waitSeconds) + L" 秒";

    case BlockType::If:
        switch (b.condition)
        {
        case ConditionType::And:
            return L"もし (" +
                GetConditionName(
                    b.leftCondition,
                    b.leftVariableName,
                    b.leftConditionValue,
                    b.booleanVariableName
                ) +
                L") AND (" +
                GetConditionName(
                    b.rightCondition,
                    b.rightVariableName,
                    b.rightConditionValue,
                    b.booleanVariableName
                ) + L")";

        case ConditionType::Or:
            return L"もし (" +
                GetConditionName(
                    b.leftCondition,
                    b.leftVariableName,
                    b.leftConditionValue,
                    b.booleanVariableName
                ) +
                L") OR (" +
                GetConditionName(
                    b.rightCondition,
                    b.rightVariableName,
                    b.rightConditionValue,
                    b.booleanVariableName
                ) + L")";

        case ConditionType::Not:
            return L"もし NOT (" +
                GetConditionName(
                    b.leftCondition,
                    b.leftVariableName,
                    b.leftConditionValue,
                    b.booleanVariableName
                ) + L")";

        default:
            return L"もし " +
                GetConditionName(
                    b.condition,
                    b.variableName,
                    b.conditionValue,
                    b.booleanVariableName
                ) + L" なら";
        }

    case BlockType::Else:
        return L"でなければ";

    case BlockType::EndIf:
        return L"条件ここまで";

    case BlockType::CreateVariable:
        return L"変数をつくる " +
            b.variableName + L" = " +
            std::to_wstring(b.value);

    case BlockType::SetVariable:
        return b.variableName + L" に " +
            std::to_wstring(b.value) +
            L" を設定する";

    case BlockType::ChangeVariable:
        if (b.value >= 0)
        {
            return b.variableName + L" を " +
                std::to_wstring(b.value) +
                L" 増やす";
        }

        return b.variableName + L" を " +
            std::to_wstring(
                -static_cast<long long>(b.value)
            ) + L" 減らす";

    case BlockType::AddVariable:
        return b.variableName + L" に " +
            std::to_wstring(b.value) + L" を足す";

    case BlockType::SubtractVariable:
        return b.variableName + L" から " +
            std::to_wstring(b.value) + L" を引く";

    case BlockType::SetBoolean:
        return b.booleanVariableName +
            (b.booleanValue
                ? L" = 真（TRUE）"
                : L" = 偽（FALSE）");


        // 停止・再開
    case BlockType::Stop:
        return L"ロボットを停止";

    case BlockType::Resume:
        return L"ロボットを再開";

    case BlockType::DoNothing:
        return L"何もしない";

        // 砲塔・レーダー
    case BlockType::TurnGunRight:
        return L"砲塔を右に " +
            [&]() {
            std::wostringstream stream;
            stream << std::fixed << std::setprecision(0) << b.degrees;
            return stream.str();
            }() + L" 度旋回";

    case BlockType::TurnGunLeft:
        return L"砲塔を左に " +
            [&]() {
            std::wostringstream stream;
            stream << std::fixed << std::setprecision(0) << b.degrees;
            return stream.str();
            }() + L" 度旋回";

    case BlockType::TurnRadarRight:
        return L"レーダーを右に " +
            [&]() {
            std::wostringstream stream;
            stream << std::fixed << std::setprecision(0) << b.degrees;
            return stream.str();
            }() + L" 度旋回";

    case BlockType::TurnRadarLeft:
        return L"レーダーを左に " +
            [&]() {
            std::wostringstream stream;
            stream << std::fixed << std::setprecision(0) << b.degrees;
            return stream.str();
            }() + L" 度旋回";


    case BlockType::SetAdjustGunForRobotTurn:
        return L"車体旋回時に砲塔を独立させる";

    case BlockType::SetAdjustRadarForRobotTurn:
        return L"車体旋回時にレーダーを独立させる";

    case BlockType::SetAdjustRadarForGunTurn:
        return L"砲塔旋回時にレーダーを独立させる";

        // 戦闘ロジック
    case BlockType::KeepAwayRadarFire:
        return L"敵を追跡して攻撃";

    case BlockType::BodyBlowFire:
        return L"敵に接近して攻撃";

    case BlockType::SmartFire:
        return L"距離に応じて弾の威力を変更";

    case BlockType::GetAwayFromWall:
        return L"壁から離れる";

    case BlockType::TurnBodyToEnemy:
        return L"車体を敵に向ける";

    case BlockType::TurnGunToEnemy:
        return L"砲塔を敵に向ける";

    case BlockType::HesitateNextBullet:
        return L"敵の弾を避ける";

        // 勝利時の動作
    case BlockType::VictoryDance:
        return L"勝利のダンス";

    case BlockType::VictorySpin:
        return L"勝利時に回転";

        // 色変更
    case BlockType::SetBodyColor:
        return L"本体の色を変更";

    case BlockType::SetGunColor:
        return L"砲塔の色を変更";

    case BlockType::SetRadarColor:
        return L"レーダーの色を変更";

    case BlockType::SetBulletColor:
        return L"弾の色を変更";

    case BlockType::SetScanColor:
        return L"索敵の色を変更";

        // ゲーム情報取得
    case BlockType::GetName:
        return L"ロボット名を取得";

    case BlockType::GetX:
        return L"X座標を取得";

    case BlockType::GetY:
        return L"Y座標を取得";

    case BlockType::GetEnergy:
        return L"エネルギーを取得";

    case BlockType::GetHeading:
        return L"車体の向きを取得";

    case BlockType::GetGunHeading:
        return L"砲塔の向きを取得";

    case BlockType::GetRadarHeading:
        return L"レーダーの向きを取得";

    case BlockType::GetGunCoolingRate:
        return L"砲塔の冷却速度を取得";

    case BlockType::GetGunHeat:
        return L"砲塔の温度を取得";

    case BlockType::GetVelocity:
        return L"速度を取得";

    case BlockType::GetOthers:
        return L"残りの敵数を取得";

    case BlockType::GetBattleFieldHeight:
        return L"フィールドの高さを取得";

    case BlockType::GetBattleFieldWidth:
        return L"フィールドの幅を取得";

    case BlockType::GetHeight:
        return L"ロボットの高さを取得";

    case BlockType::GetWidth:
        return L"ロボットの幅を取得";

    case BlockType::GetNumRounds:
        return L"総ラウンド数を取得";

    case BlockType::GetRoundNum:
        return L"現在のラウンド数を取得";

    case BlockType::GetTime:
        return L"経過時間を取得";
        // 特殊行動系
    case BlockType::FireBulletAndGetResult:
        return L"弾を撃って結果を取得";

    case BlockType::Scan:
        return L"索敵する";

        // 関数系
    case BlockType::ForwardBackwardGun360:
        return L"前後移動しながら砲塔を360度回す";

    case BlockType::ZigzagMove:
        return L"左右にジグザグ移動する";

        // イベントブロックの表示名（画面に表示する名前）
    case BlockType::OnScannedRobot:
        return L"敵ロボットを見つけたとき";
    case BlockType::OnHitWall:
        return L"壁にぶつかったとき";
    case BlockType::OnBulletHit:
        return L"撃った弾が敵に当たったとき";
    case BlockType::OnHitByBullet:
        return L"敵の弾に当たったとき";
    case BlockType::OnHitRobot:
        return L"ロボット同士がぶつかったとき";
    case BlockType::OnWin:
        return L"勝利したとき";
    }

    return L"不明";
}

// ============================================================
// ブロック名：種類だけで取得
// ============================================================

std::wstring GetBlockName(BlockType type)
{
    Block block{};
    block.type = type;

    return GetBlockName(block);
}

// ============================================================
// フォントを小さくする対象ブロック
// ============================================================

static bool IsSmallFontBlock(BlockType type)
{
    switch (type)
    {
    case BlockType::CreateVariable:
    case BlockType::SetVariable:
    case BlockType::TurnGunRight:
    case BlockType::TurnGunLeft:
    case BlockType::TurnRadarRight:
    case BlockType::TurnRadarLeft:
    case BlockType::SetRadarColor:
        return true;

    default:
        return false;
    }
}

// ============================================================
// 横幅を広げる対象ブロック
// ============================================================

static bool IsWideBlock(BlockType type)
{
    switch (type)
    {
    case BlockType::SmartFire:
    case BlockType::ForwardBackwardGun360:
    case BlockType::SetAdjustGunForRobotTurn:
    case BlockType::SetAdjustRadarForRobotTurn:
    case BlockType::SetAdjustRadarForGunTurn:
    case BlockType::GetBattleFieldHeight:
    case BlockType::GetBattleFieldWidth:
    case BlockType::TurnRadarRight:
    case BlockType::TurnRadarLeft:
        return true;

    default:
        return false;
    }
}

// ============================================================
// 当たり判定
// ============================================================

bool HitBlock(const Block& b, int x, int y)
{
    // DrawBlock() renders wide blocks at 250 px. The hitbox must use that
    // same width; otherwise clicking a visible selected wide block can miss
    // it and the editor treats the click as the start of a new range select.
    const int hitWidth = IsWideBlock(b.type)
        ? (std::max)(b.w, 250)
        : b.w;

    return
        x >= b.x &&
        x <= b.x + hitWidth &&
        y >= b.y &&
        y <= b.y + b.h;
}

// ============================================================
// ブロック描画
// ============================================================

void DrawBlock(HDC hdc, const Block& b, bool glowing)
{
    if (!hdc)
        return;

    const int tabH = 8;

    // ========================================================
    // 描画幅
    // ========================================================
    const int drawWidth = IsWideBlock(b.type)
        ? (std::max)(b.w, 250)
        : b.w;

    // ========================================================
    // ブロックの色
    // ========================================================
    COLORREF blockColor = IsUserFunctionCall(b)
        ? RGB(220, 90, 150)
        : GetBlockCategoryColor(b.type);

    if (glowing)
        blockColor = MakeBrighterColor(blockColor);

    HBRUSH brush = CreateSolidBrush(blockColor);
    if (!brush)
        return;

    HPEN pen = CreatePen(
        PS_SOLID,
        1,
        RGB(255, 255, 255)
    );

    if (!pen)
    {
        DeleteObject(brush);
        return;
    }

    HGDIOBJ oldBrush = SelectObject(hdc, brush);
    HGDIOBJ oldPen = SelectObject(hdc, pen);

    // ========================================================
    // Repeat：C字型ブロック
    // ========================================================
    const bool isEventContainer =
        b.type == BlockType::OnScannedRobot ||
        b.type == BlockType::OnHitWall ||
        b.type == BlockType::OnBulletHit ||
        b.type == BlockType::OnHitByBullet ||
        b.type == BlockType::OnHitRobot ||
        b.type == BlockType::OnWin;

    if (b.type == BlockType::Repeat || isEventContainer)
    {
        const int x = b.x;
        const int y = b.y;
        // Container drawing must use the same effective width as hit-testing.
        // Event blocks are wide containers even when their stored width is smaller.
        const int w = drawWidth;
        const int h = (std::max)(b.h, 120);

        const int tabW = 40;
        const int tabX = 24;
        const int wallW = 24;
        const int headerH = 46;
        const int bottomH = isEventContainer ? 12 : 30;

        std::vector<POINT> points = { { x, y } };
        if (!isEventContainer)
        {
            points.push_back({ x + tabX, y });
            points.push_back({ x + tabX, y + tabH });
            points.push_back({ x + tabX + tabW, y + tabH });
            points.push_back({ x + tabX + tabW, y });
        }
        points.push_back({ x + w, y });
        points.push_back({ x + w, y + headerH });
        points.push_back({ x + wallW, y + headerH });
        points.push_back({ x + wallW, y + h - bottomH });
        if (!isEventContainer)
        {
            points.push_back({ x + wallW + 24, y + h - bottomH });
            points.push_back({ x + wallW + 24, y + h - bottomH + 8 });
            points.push_back({ x + wallW + 64, y + h - bottomH + 8 });
            points.push_back({ x + wallW + 64, y + h - bottomH });
        }
        // Both container types have a complete bottom rail. Only Repeat has
        // the output tab; event blocks must not look stack-connectable.
        points.push_back({ x + w, y + h - bottomH });
        points.push_back({ x + w, y + h });
        if (!isEventContainer)
        {
            points.push_back({ x + tabX + tabW, y + h });
            points.push_back({ x + tabX + tabW, y + h + tabH });
            points.push_back({ x + tabX, y + h + tabH });
            points.push_back({ x + tabX, y + h });
        }
        points.push_back({ x, y + h });

        Polygon(hdc, points.data(), static_cast<int>(points.size()));

        // 内側を背景色で塗る
        HBRUSH bgBrush =
            CreateSolidBrush(RGB(245, 245, 245));

        if (bgBrush)
        {
            RECT inner =
            {
                x + wallW + 1,
                y + headerH + 1,
                x + w - 1,
                y + h - bottomH - 1
            };

            FillRect(hdc, &inner, bgBrush);
            DeleteObject(bgBrush);
        }

        // 内側上部の下向き接続タブ
        POINT innerTab[] =
        {
            { x + wallW + 24, y + headerH - 1 },
            { x + wallW + 64, y + headerH - 1 },
            { x + wallW + 64, y + headerH + tabH },
            { x + wallW + 24, y + headerH + tabH }
        };

        Polygon(
            hdc,
            innerTab,
            sizeof(innerTab) / sizeof(innerTab[0])
        );

        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(255, 255, 255));



        if (isEventContainer)
        {
            const std::wstring eventName = GetBlockName(b);
            RECT eventTextRect = { x + 12, y + 6, x + w - 8, y + headerH - 2 };

            // 特定のイベントブロックだけフォントを小さくする
            const int eventFontHeight =
                (b.type == BlockType::OnBulletHit ||
                    b.type == BlockType::OnHitRobot)
                ? -12
                : -15;

            HFONT eventFont = CreateFontW(eventFontHeight, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Meiryo UI");
            HGDIOBJ oldFont = eventFont ? SelectObject(hdc, eventFont) : nullptr;
            DrawTextW(hdc, eventName.c_str(), -1, &eventTextRect,
                DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            if (eventFont)
            {
                SelectObject(hdc, oldFont);
                DeleteObject(eventFont);
            }
        }
        else
        {
            const std::wstring repeatPrefix = L"繰り返し ";
            const std::wstring repeatNumber =
                std::to_wstring(b.repeatCount);
            const std::wstring repeatSuffix = L" 回";

            // 「繰り返し」の文字を描画
            RECT repeatTextRect =
            {
                x + 12,
                y + 8,
                x + w - 8,
                y + headerH
            };

            DrawTextW(
                hdc,
                repeatPrefix.c_str(),
                -1,
                &repeatTextRect,
                DT_LEFT | DT_VCENTER | DT_SINGLELINE
            );

            // 数値の位置と幅を計算
            SIZE prefixSize{};
            SIZE numberSize{};

            GetTextExtentPoint32W(
                hdc,
                repeatPrefix.c_str(),
                static_cast<int>(repeatPrefix.size()),
                &prefixSize
            );

            GetTextExtentPoint32W(
                hdc,
                repeatNumber.c_str(),
                static_cast<int>(repeatNumber.size()),
                &numberSize
            );

            const int boxLeft = x + 12 + prefixSize.cx;
            const int boxWidth = (std::max)(
                static_cast<int>(numberSize.cx) + 4,
                30
                );

            const int boxTop = y + 3 + (headerH - 24) / 2;

            RECT numberBox =
            {
                boxLeft,
                boxTop,
                boxLeft + boxWidth,
                boxTop + 24
            };

            // 白い入力枠を描画
            HBRUSH whiteBrush =
                CreateSolidBrush(RGB(255, 255, 255));

            HPEN borderPen =
                CreatePen(PS_SOLID, 1, RGB(190, 190, 190));

            if (whiteBrush && borderPen)
            {
                HGDIOBJ oldBoxBrush = SelectObject(hdc, whiteBrush);
                HGDIOBJ oldBoxPen = SelectObject(hdc, borderPen);

                RoundRect(
                    hdc,
                    numberBox.left,
                    numberBox.top,
                    numberBox.right,
                    numberBox.bottom,
                    8,
                    8
                );

                SelectObject(hdc, oldBoxPen);
                SelectObject(hdc, oldBoxBrush);
            }

            if (whiteBrush)
                DeleteObject(whiteBrush);

            if (borderPen)
                DeleteObject(borderPen);

            // 白い枠の中に数値を描画
            SetTextColor(hdc, RGB(30, 30, 30));

            RECT numberTextRect = numberBox;
            numberTextRect.left += 2;
            numberTextRect.right -= 2;

            DrawTextW(
                hdc,
                repeatNumber.c_str(),
                -1,
                &numberTextRect,
                DT_CENTER | DT_VCENTER |
                DT_SINGLELINE | DT_NOPREFIX
            );

            // 「回」を白文字で描画
            SetTextColor(hdc, RGB(255, 255, 255));

            RECT suffixRect = repeatTextRect;
            suffixRect.left = boxLeft + boxWidth + 3;

            DrawTextW(
                hdc,
                repeatSuffix.c_str(),
                -1,
                &suffixRect,
                DT_LEFT | DT_VCENTER | DT_SINGLELINE
            );


        }
        SelectObject(hdc, oldPen);
        SelectObject(hdc, oldBrush);

        DeleteObject(pen);
        DeleteObject(brush);

        return;
    }

    // ========================================================
    // 通常ブロックの本体
    // ========================================================
    POINT points[] =
    {
        { b.x,             b.y },
        { b.x + 24,        b.y },
        { b.x + 24,        b.y + tabH },
        { b.x + 64,        b.y + tabH },
        { b.x + 64,        b.y },
        { b.x + drawWidth, b.y },
        { b.x + drawWidth, b.y + b.h },
        { b.x,             b.y + b.h }
    };

    Polygon(
        hdc,
        points,
        sizeof(points) / sizeof(points[0])
    );

    // 下側の接続用タブ
    RECT tab =
    {
        b.x + 24,
        b.y + b.h - 1,
        b.x + 64,
        b.y + b.h + tabH
    };

    FillRect(hdc, &tab, brush);

    // ========================================================
    // ブロック名とフォント
    // ========================================================
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(255, 255, 255));

    const std::wstring name = GetBlockName(b);

    const int fontHeight =
        (b.type == BlockType::ForwardBackwardGun360) ? -14 :
        IsSmallFontBlock(b.type) ? -14 : -16;

    HFONT font = CreateFontW(
        fontHeight,
        0,
        0,
        0,
        FW_NORMAL,
        FALSE,
        FALSE,
        FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        L"Meiryo UI"
    );

    HGDIOBJ oldFont = nullptr;

    if (font)
        oldFont = SelectObject(hdc, font);

    RECT textRect =
    {
        b.x + 12,
        b.y + 10,
        b.x + drawWidth - 8,
        b.y + b.h - 4
    };

    // ========================================================
    // 数値入力欄に表示する文字列
    // ========================================================
    std::wstring numericText;

    switch (b.type)
    {
    case BlockType::Move:
    case BlockType::MoveBackward:
    case BlockType::MoveRight:
    case BlockType::MoveLeft:
        numericText = std::to_wstring(b.distance);
        break;

    case BlockType::Repeat:
        numericText = std::to_wstring(b.repeatCount);
        break;

    case BlockType::Wait:
        numericText = FormatNumber(b.waitSeconds);
        break;

    case BlockType::TurnRight:
    case BlockType::TurnLeft:
        numericText = FormatNumber(b.degrees);
        break;

   case BlockType::TurnGunRight:
case BlockType::TurnGunLeft:
case BlockType::TurnRadarRight:
case BlockType::TurnRadarLeft:
{
    wchar_t buffer[64]{};
    swprintf_s(buffer, L"%.0f", b.degrees);
    numericText = buffer;
    break;
}

    case BlockType::FireBullet:
        numericText = FormatNumber(b.power);
        break;

    case BlockType::CreateVariable:
    case BlockType::SetVariable:
    case BlockType::AddVariable:
    case BlockType::SubtractVariable:
        numericText = std::to_wstring(b.value);
        break;

    case BlockType::ChangeVariable:
        numericText = std::to_wstring(
            b.value >= 0
            ? static_cast<long long>(b.value)
            : -static_cast<long long>(b.value)
        );
        break;

    case BlockType::If:
        numericText = std::to_wstring(b.conditionValue);
        break;

    default:
        break;
    }

    // ========================================================
    // 数値の前後を分割して描画
    // 変数ブロックと「指定秒数待つ」が対象
    // ========================================================
    const bool isVariableBlock =
        b.type == BlockType::SetVariable ||
        b.type == BlockType::ChangeVariable ||
        b.type == BlockType::AddVariable ||
        b.type == BlockType::SubtractVariable;

    
    const bool isRotationBlock =
        b.type == BlockType::TurnGunRight ||
        b.type == BlockType::TurnGunLeft ||
        b.type == BlockType::TurnRadarRight ||
        b.type == BlockType::TurnRadarLeft;

    const bool shouldSplitText =
        isVariableBlock ||
        b.type == BlockType::Wait;
    

    bool drewSplitText = false;

    SIZE prefixSize{};
    SIZE numberSize{};

    int boxLeft = 0;
    int boxWidth = 0;

    if (shouldSplitText && !numericText.empty())
    {
        const size_t numberPos = name.rfind(numericText);

        if (numberPos != std::wstring::npos)
        {
            const std::wstring prefix =
                name.substr(0, numberPos);

            const std::wstring suffix =
                name.substr(numberPos + numericText.size());

            GetTextExtentPoint32W(
                hdc,
                prefix.c_str(),
                static_cast<int>(prefix.size()),
                &prefixSize
            );

            GetTextExtentPoint32W(
                hdc,
                numericText.c_str(),
                static_cast<int>(numericText.size()),
                &numberSize
            );

           

            // 旋回ブロックは「前進する」と同じ横位置にする
            if (isRotationBlock)
            {
                const wchar_t* referenceText = L"前進する  ";

                // 「前進する」と同じフォントサイズで幅を計測
                HFONT referenceFont = CreateFontW(
                    -16, 0, 0, 0, FW_NORMAL,
                    FALSE, FALSE, FALSE,
                    DEFAULT_CHARSET,
                    OUT_DEFAULT_PRECIS,
                    CLIP_DEFAULT_PRECIS,
                    CLEARTYPE_QUALITY,
                    DEFAULT_PITCH | FF_DONTCARE,
                    L"Meiryo UI"
                );

                if (referenceFont)
                {
                    HGDIOBJ oldReferenceFont =
                        SelectObject(hdc, referenceFont);

                    SIZE referenceSize{};

                    GetTextExtentPoint32W(
                        hdc,
                        referenceText,
                        static_cast<int>(wcslen(referenceText)),
                        &referenceSize
                    );

                    boxLeft = b.x + 12 + referenceSize.cx;

                    SelectObject(hdc, oldReferenceFont);
                    DeleteObject(referenceFont);
                }
            }
            else
            {
                boxLeft = b.x + 12 + prefixSize.cx;
            }

            boxWidth = (std::max)(
                static_cast<int>(numberSize.cx) + 4,
                30
                );


            // 数値の前の文字は元の位置に描画
            RECT prefixRect = textRect;

            DrawTextW(
                hdc,
                prefix.c_str(),
                -1,
                &prefixRect,
                DT_LEFT | DT_VCENTER | DT_SINGLELINE
            );

            // 数値より後ろの文字だけを右へ移動
            RECT suffixRect = textRect;

            suffixRect.left =
                boxLeft + boxWidth + 3;

            DrawTextW(
                hdc,
                suffix.c_str(),
                -1,
                &suffixRect,
                DT_LEFT | DT_VCENTER |
                DT_SINGLELINE | DT_END_ELLIPSIS
            );

            drewSplitText = true;
        }
    }

    // 分割対象ではないブロックは全文を従来どおり描画
    if (!drewSplitText)
    {
        DrawTextW(
            hdc,
            name.c_str(),
            -1,
            &textRect,
            DT_LEFT | DT_VCENTER |
            DT_SINGLELINE | DT_END_ELLIPSIS
        );
    }

    // ========================================================
    // 数値の白い入力欄
    // ========================================================
    if (!numericText.empty())
    {
        const size_t numberPos = name.rfind(numericText);

        if (numberPos != std::wstring::npos)
        {
            if (!drewSplitText)
            {
                const std::wstring prefix =
                    name.substr(0, numberPos);

                GetTextExtentPoint32W(
                    hdc,
                    prefix.c_str(),
                    static_cast<int>(prefix.size()),
                    &prefixSize
                );

                GetTextExtentPoint32W(
                    hdc,
                    numericText.c_str(),
                    static_cast<int>(numericText.size()),
                    &numberSize
                );

                boxLeft = b.x + 12 + prefixSize.cx;
                boxWidth = (std::max)(
                    static_cast<int>(numberSize.cx) + 4,
                    30
                    );
            }

            const int boxTop =
                b.y + (b.h - 24) / 2;

            RECT numberBox =
            {
                boxLeft,
                boxTop,
                boxLeft + boxWidth,
                boxTop + 24
            };

            // 白い入力欄
            HBRUSH whiteBrush =
                CreateSolidBrush(RGB(255, 255, 255));

            HPEN borderPen =
                CreatePen(
                    PS_SOLID,
                    1,
                    RGB(190, 190, 190)
                );

            if (whiteBrush && borderPen)
            {
                HGDIOBJ oldBoxBrush =
                    SelectObject(hdc, whiteBrush);

                HGDIOBJ oldBoxPen =
                    SelectObject(hdc, borderPen);

                RoundRect(
                    hdc,
                    numberBox.left,
                    numberBox.top,
                    numberBox.right,
                    numberBox.bottom,
                    8,
                    8
                );

                SelectObject(hdc, oldBoxPen);
                SelectObject(hdc, oldBoxBrush);
            }

            if (whiteBrush)
                DeleteObject(whiteBrush);

            if (borderPen)
                DeleteObject(borderPen);

            // 白い入力欄の中に数値を黒色で描画
            SetTextColor(hdc, RGB(30, 30, 30));

            RECT numberTextRect = numberBox;
            numberTextRect.left += 3;
            numberTextRect.right -= 3;

            DrawTextW(
                hdc,
                numericText.c_str(),
                -1,
                &numberTextRect,
                DT_CENTER |
                DT_VCENTER |
                DT_SINGLELINE |
                DT_NOPREFIX
            );

            SetTextColor(hdc, RGB(255, 255, 255));
        }
    }

    // ========================================================
    // フォントを元に戻して解放
    // ========================================================
    if (font)
    {
        SelectObject(hdc, oldFont);
        DeleteObject(font);
    }

    // ========================================================
    // 色変更ブロックの色見本
    // ========================================================
    COLORREF previewColor = RGB(0, 0, 0);
    bool showColorPreview = true;

    switch (b.type)
    {
    case BlockType::SetBodyColor:
        previewColor = b.bodyColor;
        break;

    case BlockType::SetGunColor:
        previewColor = b.gunColor;
        break;

    case BlockType::SetRadarColor:
        previewColor = b.radarColor;
        break;

    case BlockType::SetBulletColor:
        previewColor = b.bulletColor;
        break;

    case BlockType::SetScanColor:
        previewColor = b.scanColor;
        break;

    default:
        showColorPreview = false;
        break;
    }

    if (showColorPreview)
    {
        RECT colorRect =
        {
            b.x + drawWidth - 34,
            b.y + 17,
            b.x + drawWidth - 12,
            b.y + 39
        };

        HBRUSH colorBrush =
            CreateSolidBrush(previewColor);

        if (colorBrush)
        {
            FillRect(hdc, &colorRect, colorBrush);
            DeleteObject(colorBrush);
        }

        HBRUSH borderBrush =
            CreateSolidBrush(RGB(255, 255, 255));

        if (borderBrush)
        {
            FrameRect(hdc, &colorRect, borderBrush);
            DeleteObject(borderBrush);
        }
    }

    // ========================================================
    // GDIオブジェクトを元に戻して解放
    // ========================================================
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);

    DeleteObject(pen);
    DeleteObject(brush);
}


// ============================================================
// If / Else / EndIf の対応位置
// ============================================================

struct IfParts
{
    size_t elseIndex;
    size_t endIfIndex;
};

static IfParts FindIfParts(
    const std::vector<Block>& blocks,
    size_t begin,
    size_t end)
{
    int depth = 0;
    IfParts result = { end, end };

    for (size_t i = begin; i < end; ++i)
    {
        if (blocks[i].type == BlockType::If)
        {
            ++depth;
        }
        else if (blocks[i].type == BlockType::EndIf)
        {
            if (depth == 0)
            {
                result.endIfIndex = i;
                return result;
            }

            --depth;
        }
        else if (
            blocks[i].type == BlockType::Else &&
            depth == 0)
        {
            if (result.elseIndex == end)
                result.elseIndex = i;
        }
    }

    return result;
}

// ============================================================
// Repeat / EndRepeat の対応位置
// ============================================================

struct RepeatParts
{
    size_t endIndex;
};

static RepeatParts FindRepeatParts(
    const std::vector<Block>& blocks,
    size_t begin,
    size_t end)
{
    int depth = 1;

    for (size_t i = begin; i < end; ++i)
    {
        if (blocks[i].type == BlockType::Repeat)
        {
            ++depth;
        }
        else if (blocks[i].type == BlockType::EndRepeat)
        {
            --depth;

            if (depth == 0)
                return { i };
        }
    }

    return { end };
}

// ============================================================
// 整数の安全な加算
// ============================================================

static int SafeAddInt(int current, int amount)
{
    const long long result =
        static_cast<long long>(current) +
        static_cast<long long>(amount);

    const long long minimum =
        (std::numeric_limits<int>::min)();

    const long long maximum =
        (std::numeric_limits<int>::max)();

    return static_cast<int>(
        (std::max)(
            minimum,
            (std::min)(maximum, result)
            )
        );
}

// ============================================================
// 整数の安全な減算
// ============================================================

static int SafeSubtractInt(int current, int amount)
{
    const long long result =
        static_cast<long long>(current) -
        static_cast<long long>(amount);

    const long long minimum =
        (std::numeric_limits<int>::min)();

    const long long maximum =
        (std::numeric_limits<int>::max)();

    return static_cast<int>(
        (std::max)(
            minimum,
            (std::min)(maximum, result)
            )
        );
}

// ============================================================
// 基本条件の判定
// ============================================================

static bool CheckSimpleCondition(
    ConditionType condition,
    const std::wstring& variableName,
    int conditionValue,
    const std::wstring& booleanVariableName,
    const RobotVariables& variables,
    const RobotBooleanVariables& booleanVariables,
    const std::function<bool(const Block&)>& conditionCheck)
{
    switch (condition)
    {
    case ConditionType::Always:
    case ConditionType::TrueValue:
        return true;

    case ConditionType::FalseValue:
        return false;

        // 数値変数の比較
    case ConditionType::VariableAtLeast:
    case ConditionType::VariableAtMost:
    case ConditionType::VariableEqual:
    case ConditionType::VariableNotEqual:
    case ConditionType::VariableGreaterThan:
    case ConditionType::VariableLessThan:
    {
        const auto it = variables.find(variableName);

        if (it == variables.end())
            return false;

        const int current = it->second;

        switch (condition)
        {
        case ConditionType::VariableAtLeast:
            return current >= conditionValue;

        case ConditionType::VariableAtMost:
            return current <= conditionValue;

        case ConditionType::VariableEqual:
            return current == conditionValue;

        case ConditionType::VariableNotEqual:
            return current != conditionValue;

        case ConditionType::VariableGreaterThan:
            return current > conditionValue;

        case ConditionType::VariableLessThan:
            return current < conditionValue;

        default:
            return false;
        }
    }

    // 真偽値変数
    case ConditionType::BooleanVariable:
    {
        const auto it =
            booleanVariables.find(booleanVariableName);

        if (it == booleanVariables.end())
            return false;

        return it->second;
    }

    // 外部センサーによる条件判定
    case ConditionType::HealthAtLeast:
    case ConditionType::EnemyDetected:
    case ConditionType::EnemyDistanceAtMost:
    case ConditionType::EnemyOnLeft:
    case ConditionType::EnemyOnRight:
    case ConditionType::EnemyAhead:
    case ConditionType::EnemyBehind:
    case ConditionType::WallNear:
    case ConditionType::EnergyAtMost:
    {
        if (!conditionCheck)
            return false;

        Block temp{};
        temp.type = BlockType::If;
        temp.condition = condition;
        temp.variableName = variableName;
        temp.conditionValue = conditionValue;

        return conditionCheck(temp);
    }

    case ConditionType::And:
    case ConditionType::Or:
    case ConditionType::Not:
        return false;
    }

    return false;
}

// ============================================================
// 条件判定：AND / OR / NOT
// ============================================================

static bool CheckCondition(
    const Block& block,
    const RobotVariables& variables,
    const RobotBooleanVariables& booleanVariables,
    const std::function<bool(const Block&)>& conditionCheck)
{
    switch (block.condition)
    {
    case ConditionType::And:
    {
        const bool left = CheckSimpleCondition(
            block.leftCondition,
            block.leftVariableName,
            block.leftConditionValue,
            block.booleanVariableName,
            variables,
            booleanVariables,
            conditionCheck
        );

        if (!left)
            return false;

        const bool right = CheckSimpleCondition(
            block.rightCondition,
            block.rightVariableName,
            block.rightConditionValue,
            block.booleanVariableName,
            variables,
            booleanVariables,
            conditionCheck
        );

        return left && right;
    }

    case ConditionType::Or:
    {
        const bool left = CheckSimpleCondition(
            block.leftCondition,
            block.leftVariableName,
            block.leftConditionValue,
            block.booleanVariableName,
            variables,
            booleanVariables,
            conditionCheck
        );

        if (left)
            return true;

        const bool right = CheckSimpleCondition(
            block.rightCondition,
            block.rightVariableName,
            block.rightConditionValue,
            block.booleanVariableName,
            variables,
            booleanVariables,
            conditionCheck
        );

        return left || right;
    }

    case ConditionType::Not:
    {
        const bool result = CheckSimpleCondition(
            block.leftCondition,
            block.leftVariableName,
            block.leftConditionValue,
            block.booleanVariableName,
            variables,
            booleanVariables,
            conditionCheck
        );

        return !result;
    }

    default:
        return CheckSimpleCondition(
            block.condition,
            block.variableName,
            block.conditionValue,
            block.booleanVariableName,
            variables,
            booleanVariables,
            conditionCheck
        );
    }
}

// ============================================================
// ブロック列の内部実行
// ============================================================

static void ExecuteRange(
    const std::vector<Block>& blocks,
    size_t begin,
    size_t end,
    const std::function<void(const Block&)>& executeOne,
    const std::function<bool(const Block&)>& conditionCheck,
    RobotVariables& variables,
    RobotBooleanVariables& booleanVariables)
{
    size_t i = begin;

    while (i < end)
    {
        const Block& block = blocks[i];

        // 数値変数を作成
        if (block.type == BlockType::CreateVariable)
        {
            variables.emplace(
                block.variableName,
                block.value
            );

            ++i;
            continue;
        }

        // 数値変数に値を設定
        if (block.type == BlockType::SetVariable)
        {
            variables[block.variableName] = block.value;

            ++i;
            continue;
        }

        // 真偽値を設定
        if (block.type == BlockType::SetBoolean)
        {
            booleanVariables[block.booleanVariableName] =
                block.booleanValue;

            ++i;
            continue;
        }

        // 変数の増減・足し算・引き算
        if (block.type == BlockType::ChangeVariable ||
            block.type == BlockType::AddVariable ||
            block.type == BlockType::SubtractVariable)
        {
            auto it = variables.find(block.variableName);

            // 未作成の数値変数は変更しない
            if (it != variables.end())
            {
                if (block.type == BlockType::SubtractVariable)
                {
                    it->second = SafeSubtractInt(
                        it->second,
                        block.value
                    );
                }
                else
                {
                    it->second = SafeAddInt(
                        it->second,
                        block.value
                    );
                }
            }

            ++i;
            continue;
        }

        // Repeat
        if (block.type == BlockType::Repeat)
        {
            const RepeatParts parts =
                FindRepeatParts(blocks, i + 1, end);

            // EndRepeat がなければ中断
            if (parts.endIndex == end)
                return;

            const int count = (std::max)(
                0,
                (std::min)(block.repeatCount, 1000)
                );

            for (int n = 0; n < count; ++n)
            {
                ExecuteRange(
                    blocks,
                    i + 1,
                    parts.endIndex,
                    executeOne,
                    conditionCheck,
                    variables,
                    booleanVariables
                );
            }

            i = parts.endIndex + 1;
            continue;
        }

        // 単独の EndRepeat は無視
        if (block.type == BlockType::EndRepeat)
        {
            ++i;
            continue;
        }

        // If
        if (block.type == BlockType::If)
        {
            const IfParts parts =
                FindIfParts(blocks, i + 1, end);

            // EndIf がなければ中断
            if (parts.endIfIndex == end)
                return;

            const bool met = CheckCondition(
                block,
                variables,
                booleanVariables,
                conditionCheck
            );

            if (met)
            {
                const size_t trueEnd =
                    parts.elseIndex != end
                    ? parts.elseIndex
                    : parts.endIfIndex;

                ExecuteRange(
                    blocks,
                    i + 1,
                    trueEnd,
                    executeOne,
                    conditionCheck,
                    variables,
                    booleanVariables
                );
            }
            else if (parts.elseIndex != end)
            {
                ExecuteRange(
                    blocks,
                    parts.elseIndex + 1,
                    parts.endIfIndex,
                    executeOne,
                    conditionCheck,
                    variables,
                    booleanVariables
                );
            }

            i = parts.endIfIndex + 1;
            continue;
        }

        // 構造用ブロック
        if (block.type == BlockType::Else ||
            block.type == BlockType::EndIf)
        {
            ++i;
            continue;
        }

        // 通常ブロック
        executeOne(block);

        // Wait
        if (block.type == BlockType::Wait)
        {
            const double seconds = (std::max)(
                0.0,
                (std::min)(block.waitSeconds, 3600.0)
                );

            if (seconds > 0.0)
            {
                std::this_thread::sleep_for(
                    std::chrono::duration<double>(seconds)
                );
            }
        }

        ++i;
    }
}

// ============================================================
// ブロック列の実行：変数を新規作成
// ============================================================

void ExecuteBlockSequence(
    const std::vector<Block>& blocks,
    const std::function<void(const Block&)>& executeOne,
    const std::function<bool(const Block&)>& conditionCheck)
{
    RobotVariables variables;
    RobotBooleanVariables booleanVariables;

    ExecuteBlockSequence(
        blocks,
        executeOne,
        conditionCheck,
        variables,
        booleanVariables
    );
}

// ============================================================
// ブロック列の実行：数値変数を共有
// ============================================================

void ExecuteBlockSequence(
    const std::vector<Block>& blocks,
    const std::function<void(const Block&)>& executeOne,
    const std::function<bool(const Block&)>& conditionCheck,
    RobotVariables& variables)
{
    RobotBooleanVariables booleanVariables;

    ExecuteBlockSequence(
        blocks,
        executeOne,
        conditionCheck,
        variables,
        booleanVariables
    );
}

// ============================================================
// ブロック列の実行：数値変数と真偽値変数を共有
// ============================================================

void ExecuteBlockSequence(
    const std::vector<Block>& blocks,
    const std::function<void(const Block&)>& executeOne,
    const std::function<bool(const Block&)>& conditionCheck,
    RobotVariables& variables,
    RobotBooleanVariables& booleanVariables)
{
    if (!executeOne)
        return;

    ExecuteRange(
        blocks,
        0,
        blocks.size(),
        executeOne,
        conditionCheck,
        variables,
        booleanVariables
    );
}

// ============================================================
// イベント名
// ============================================================


std::wstring GetEventName(RobotEvent event)
{
    switch (event)
    {
    case RobotEvent::Start:
        return L"ロボットの実行開始";

    case RobotEvent::EnemyDetected:
        return L"敵を発見したとき";

    case RobotEvent::Damaged:
        return L"被弾したとき";

    case RobotEvent::Hit:
        return L"弾が命中したとき";

    case RobotEvent::Collision:
        return L"衝突したとき";

    case RobotEvent::WallHit:
        return L"壁に衝突したとき";

    case RobotEvent::BulletHit:
        return L"弾が敵に命中したとき";

    case RobotEvent::BulletMissed:
        return L"弾が外れたとき";

    case RobotEvent::BulletHitBullet:
        return L"敵の弾と相殺したとき";

    case RobotEvent::Death:
        return L"自機が破壊されたとき";

    case RobotEvent::HitByBullet:
        return L"敵の弾に当たったとき";

    case RobotEvent::HitRobot:
        return L"敵と衝突したとき";

    case RobotEvent::RobotDeath:
        return L"敵が破壊されたとき";

    case RobotEvent::Win:
        return L"勝利したとき";
    }

    return L"不明なイベント";
}


// ============================================================
// イベントの実行：変数を新規作成
// ============================================================

void ExecuteEventSequence(
    const EventHandler& handler,
    const std::function<void(const Block&)>& executeOne,
    const std::function<bool(const Block&)>& conditionCheck)
{
    ExecuteBlockSequence(
        handler.blocks,
        executeOne,
        conditionCheck
    );
}

// ============================================================
// イベントの実行：数値変数を共有
// ============================================================

void ExecuteEventSequence(
    const EventHandler& handler,
    const std::function<void(const Block&)>& executeOne,
    const std::function<bool(const Block&)>& conditionCheck,
    RobotVariables& variables)
{
    ExecuteBlockSequence(
        handler.blocks,
        executeOne,
        conditionCheck,
        variables
    );
}

// ============================================================
// イベントの実行：数値変数と真偽値変数を共有
// ============================================================

void ExecuteEventSequence(
    const EventHandler& handler,
    const std::function<void(const Block&)>& executeOne,
    const std::function<bool(const Block&)>& conditionCheck,
    RobotVariables& variables,
    RobotBooleanVariables& booleanVariables)
{
    ExecuteBlockSequence(
        handler.blocks,
        executeOne,
        conditionCheck,
        variables,
        booleanVariables
    );
}

// ============================================================
// イベント登録のクリア
// ============================================================

void RobotEventManager::Clear()
{
    handlers.clear();
    variables.clear();
    booleanVariables.clear();
}

// ============================================================
// イベントの登録・更新
// ============================================================

void RobotEventManager::RegisterEvent(
    RobotEvent event,
    const std::vector<Block>& blocks)
{
    for (auto& handler : handlers)
    {
        if (handler.event == event)
        {
            handler.blocks = blocks;
            return;
        }
    }

    EventHandler handler;
    handler.event = event;
    handler.blocks = blocks;

    handlers.push_back(handler);
}

// ============================================================
// イベント発生
// ============================================================

void RobotEventManager::TriggerEvent(
    RobotEvent event,
    const std::function<void(const Block&)>& executeOne,
    const std::function<bool(const Block&)>& conditionCheck)
{
    if (!executeOne)
        return;

    for (const auto& handler : handlers)
    {
        if (handler.event == event)
        {
            ExecuteEventSequence(
                handler,
                executeOne,
                conditionCheck,
                variables,
                booleanVariables
            );
        }
    }
}

// ============================================================
// 登録済みイベントの取得
// ============================================================

const std::vector<EventHandler>&
RobotEventManager::GetHandlers() const
{
    return handlers;
}

// ============================================================
// 共有する数値変数の取得
// ============================================================

RobotVariables& RobotEventManager::GetVariables()
{
    return variables;
}

const RobotVariables& RobotEventManager::GetVariables() const
{
    return variables;
}

// ============================================================
// 共有する真偽値変数の取得
// ============================================================

RobotBooleanVariables&
RobotEventManager::GetBooleanVariables()
{
    return booleanVariables;
}

const RobotBooleanVariables&
RobotEventManager::GetBooleanVariables() const
{
    return booleanVariables;
}