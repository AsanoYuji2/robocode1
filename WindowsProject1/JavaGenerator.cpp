#include <windows.h>
#include <commdlg.h>
#include <fstream>
#include <algorithm>
#include <string>
#include <filesystem>
#include <cstdlib>
#include <sstream>
#include <set>

#include "JavaGenerator.h"
#include "UserFunction.h"

std::wstring SelectSavePath()
{
    wchar_t fileName[MAX_PATH] = L"BlockRobot.java";

    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = nullptr;
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_PATH;

    ofn.lpstrFilter =
        L"Java Files (*.java)\0*.java\0"
        L"All Files (*.*)\0*.*\0";

    ofn.lpstrDefExt = L"java";

    ofn.Flags =
        OFN_OVERWRITEPROMPT |
        OFN_PATHMUSTEXIST;

    if (GetSaveFileNameW(&ofn))
        return fileName;

    return L"";
}

namespace
{
    void ExpandFunctionCalls(
        const std::vector<Block>& input,
        const std::vector<UserDefinedFunction>& functions,
        std::vector<std::wstring>& callStack,
        std::vector<Block>& output,
        int depth)
    {
        // 画面上の並び順に合わせて処理する。
        std::vector<Block> ordered = input;
        std::sort(ordered.begin(), ordered.end(),
            [](const Block& a, const Block& b)
            {
                if (a.x != b.x) return a.x < b.x;
                return a.y < b.y;
            });

        // 間違った呼び出しによる無限ループを防ぐ。
        if (depth > 32)
            return;

        for (const Block& block : ordered)
        {
            if (!IsUserFunctionCall(block))
            {
                output.push_back(block);
                continue;
            }

            const std::wstring functionName = GetUserFunctionCallName(block);
            auto found = std::find_if(functions.begin(), functions.end(),
                [&](const UserDefinedFunction& function)
                {
                    return function.name == functionName;
                });

            // 削除済みなどで呼び出し先がない場合は、そのブロックを出力しない。
            if (found == functions.end())
                continue;

            // A -> B -> A のような循環呼び出しを防止。
            if (std::find(callStack.begin(), callStack.end(), functionName) != callStack.end())
                continue;

            callStack.push_back(functionName);
            ExpandFunctionCalls(found->blocks, functions, callStack, output, depth + 1);
            callStack.pop_back();
        }
    }
}

bool ExportJavaCustom(
    const std::vector<Block>& blocks,
    const std::wstring& targetFolder,
    const std::wstring& robotName)
{
    return ExportJavaCustom(blocks, targetFolder, robotName,
        std::vector<UserDefinedFunction>{});
}

struct ValidationError {
    std::wstring target;  // エラーの発生箇所
    std::wstring reason;  // エラーの理由
};

// 有効な数値かどうかを判定するヘルパー
bool IsValidNumber(double val) {
    return !std::isnan(val) && !std::isinf(val);
}

// プロジェクト全体の品質・エラーチェック関数
bool ValidateRobotProject(
    const std::vector<Block>& blocks,
    const std::wstring& folderPath,
    const std::wstring& robotName,
    const std::vector<UserDefinedFunction>& functions, // 登録されている自作関数一覧
    std::vector<ValidationError>& outErrors)
{
    outErrors.clear();

    //必須入力チェック
    if (robotName.empty()) {
        outErrors.push_back({ L"プロジェクト設定", L"ロボット名が入力されていません。" });
    }
    if (folderPath.empty()) {
        outErrors.push_back({ L"プロジェクト設定", L"ロボットの保存先フォルダが指定されていません。" });
    }
    if (blocks.empty()) {
        outErrors.push_back({ L"キャンバス", L"ブロックが1つも配置されていません。" });
    }

    // 制御構造や変数の追跡用セット
    int repeatDepth = 0;
    int ifDepth = 0;
    std::set<std::wstring> definedVariables; // 定義された変数名を記憶

    //すべてのブロックに対する詳細チェック
    for (size_t i = 0; i < blocks.size(); ++i)
    {
        const auto& block = blocks[i];

        // --- A. 自作関数の呼び出しチェック ---
        if (IsUserFunctionCall(block))
        {
            std::wstring funcName = GetUserFunctionCallName(block);
            auto it = std::find_if(functions.begin(), functions.end(), [&](const UserDefinedFunction& f) {
                return f.name == funcName;
                });
            if (it == functions.end()) {
                outErrors.push_back({ L"関数ブロック", L"存在しない関数「" + funcName + L"」が呼び出されています。" });
            }
            continue;
        }

        switch (block.type)
        {
            // --- B. 移動・旋回系の数値チェック ---
        case BlockType::Move:
        case BlockType::MoveBackward:
        case BlockType::MoveRight:
        case BlockType::MoveLeft:
            if (!IsValidNumber(block.distance)) {
                outErrors.push_back({ L"移動ブロック", L"移動距離の数値が不正です。" });
            }
            break;

        case BlockType::TurnRight:
        case BlockType::TurnLeft:
        case BlockType::TurnGunRight:
        case BlockType::TurnGunLeft:
        case BlockType::TurnRadarRight:
        case BlockType::TurnRadarLeft:
            if (!IsValidNumber(block.degrees)) {
                outErrors.push_back({ L"旋回ブロック", L"角度の数値が不正です。" });
            }
            break;

            // --- C. 攻撃系の威力（Power）チェック ---
        case BlockType::FireBullet:
        case BlockType::FireBulletAndGetResult:
            if (!IsValidNumber(block.power) || block.power <= 0.0 || block.power > 3.0) {
                outErrors.push_back({ L"射撃ブロック", L"弾の威力(Power)は 0.1 ～ 3.0 の間で指定してください。" });
            }
            break;

            // --- D. 待機時間のチェック ---
        case BlockType::Wait:
            if (!IsValidNumber(block.waitSeconds) || block.waitSeconds < 0.0) {
                outErrors.push_back({ L"待機ブロック", L"待機時間が不正（マイナス値）です。" });
            }
            break;

            // --- E. 制御構造（Repeat / If）のチェック ---
        case BlockType::Repeat:
            repeatDepth++;
            if (block.repeatCount <= 0) {
                outErrors.push_back({ L"繰り返しブロック", L"繰り返し回数は1回以上に設定してください。" });
            }
            break;

        case BlockType::EndRepeat:
            repeatDepth--;
            if (repeatDepth < 0) {
                outErrors.push_back({ L"繰り返しブロック", L"「繰り返しここまで」に対応する「繰り返す」がありません。" });
            }
            break;

        case BlockType::If:
            ifDepth++;
            break;

        case BlockType::EndIf:
            ifDepth--;
            if (ifDepth < 0) {
                outErrors.push_back({ L"条件分岐ブロック", L"「条件ここまで」に対応する「もし」がありません。" });
            }
            break;

            // --- F. 変数系のチェック ---
        case BlockType::CreateVariable:
            if (block.variableName.empty()) {
                outErrors.push_back({ L"変数ブロック", L"変数名が空です。" });
            }
            else {
                definedVariables.insert(block.variableName);
            }
            break;

        case BlockType::SetVariable:
        case BlockType::ChangeVariable:
        case BlockType::AddVariable:
        case BlockType::SubtractVariable:
            if (!block.variableName.empty() && definedVariables.find(block.variableName) == definedVariables.end()) {
                // ※ 事前作成が必須ルールの場合に警告・エラーにする場合
                // outErrors.push_back({ L"変数ブロック", L"変数「" + block.variableName + L"」が作成される前に使用されています。" });
            }
            break;

        default:
            break;
        }
    }

    // 閉じていない制御構造のチェック
    if (repeatDepth > 0) {
        outErrors.push_back({ L"制御構造", L"「繰り返す」に対して「繰り返しここまで」が不足しています。" });
    }
    if (ifDepth > 0) {
        outErrors.push_back({ L"制御構造", L"「もし」に対して「条件ここまで」が不足しています。" });
    }

    return outErrors.empty();
}
//エラーチェックの実行
bool ExportJavaCustom(
    const std::vector<Block>& blocks,
    const std::wstring& targetFolder,
    const std::wstring& robotName,
    const std::vector<UserDefinedFunction>& functions)
{
    std::vector<ValidationError> errors;
    if (!ValidateRobotProject(blocks, targetFolder, robotName, functions, errors))
    {
        std::wstring errorMsg = L"エラーが検出されたため、処理を中止しました。\n\n【検出された問題】\n";
        for (const auto& err : errors)
        {
            errorMsg += L"・ [" + err.target + L"] " + err.reason + L"\n";
        }
        MessageBoxW(nullptr, errorMsg.c_str(), L"品質エラー", MB_OK | MB_ICONWARNING);
        return false;
    }

    std::wstring debugMsg = L"保存先: " + targetFolder + L"\nロボット名: " + robotName;
    MessageBoxW(nullptr, debugMsg.c_str(), L"デバッグ確認", MB_OK);

    if (targetFolder.empty() || robotName.empty())
    {
        MessageBoxW(nullptr, L"ロボット名または保存先フォルダが入力されていません。", L"エラー", MB_OK);
        return false;
    }

    std::filesystem::path dirPath(targetFolder);
    std::filesystem::path finalJavaPath = dirPath / (robotName + L".java");

    std::string className = std::string(robotName.begin(), robotName.end());
    std::string packageName = dirPath.filename().string();

    if (packageName == "robots" || packageName.empty()) {
        packageName = "";
        dirPath = "C:\\robocode\\robots";
    }
    finalJavaPath = dirPath / (robotName + L".java");

    std::filesystem::create_directories(dirPath);

    // 自作関数の呼び出しを、その関数の中身に置き換えてからJavaコードを作る。
    // こうすることで、生成されたJava側に未知の関数名が残らない。
    std::vector<Block> ordered;
    std::vector<std::wstring> callStack;
    ExpandFunctionCalls(blocks, functions, callStack, ordered, 0);

    std::string code = "";

    // パッケージ名がある場合のみパッケージ宣言を付与する
    if (!packageName.empty())
    {
        code += "package " + packageName + ";\n\n";
    }

    // インポート文
    code +=
        "import robocode.*;\n"
        "import java.awt.Color;\n"
        "import static robocode.util.Utils.normalRelativeAngleDegrees;\n\n";

    // クラス定義
    code +=
        "public class " + className +
        " extends AdvancedRobot\n{\n"
        "    @Override\n"
        "    public void run()\n"
        "    {\n"
        "        while(true)\n"
        "        {\n";

    for (const auto& b : ordered)
    {
        switch (b.type)
        {
        case BlockType::Move:
            code +=
                "            setAhead(" +
                std::to_string(b.distance) + ");\n"
                "            execute();\n";
            break;

        case BlockType::MoveBackward:
            code +=
                "            setBack(" +
                std::to_string(b.distance) + ");\n"
                "            execute();\n";
            break;


        case BlockType::TurnRight:
            code +=
                "            setTurnRight(" +
                std::to_string(b.degrees) + ");\n"
                "            execute();\n";
            break;

        case BlockType::TurnLeft:
            code +=
                "            setTurnLeft(" +
                std::to_string(b.degrees) + ");\n"
                "            execute();\n";
            break;

        case BlockType::MoveRight:
            code +=
                "            setTurnRight(90);\n"
                "            setAhead(" +
                std::to_string(b.distance) + ");\n"
                "            setTurnLeft(90);\n"
                "            execute();\n";
            break;

        case BlockType::MoveLeft:
            code +=
                "            setTurnLeft(90);\n"
                "            setAhead(" +
                std::to_string(b.distance) + ");\n"
                "            setTurnRight(90);\n"
                "            execute();\n";
            break;

        case BlockType::FireBullet:
            code +=
                "            fire(" +
                std::to_string(b.power) + ");\n"
                "            execute();\n";
            break;
        case BlockType::FireBulletAndGetResult:
            code +=
                "            Bullet bullet = fireBullet(" +
                std::to_string(b.power) + ");\n"
                "            execute();\n";
            break;

        case BlockType::Scan:
            code +=
                "            setTurnRadarRight(360);\n"
                "            execute();\n";
            break;

        case BlockType::ForwardBackwardGun360:
            code +=
                "            setAdjustGunForRobotTurn(true);\n"
                "            setTurnGunRight(360);\n"
                "            setAhead(100);\n"
                "            execute();\n"
                "            setBack(100);\n"
                "            execute();\n";
            break;

        case BlockType::ZigzagMove:
            code +=
                "            setTurnRight(45);\n"
                "            setAhead(100);\n"
                "            execute();\n"
                "            setTurnLeft(90);\n"
                "            setAhead(100);\n"
                "            execute();\n"
                "            setTurnRight(45);\n"
                "            execute();\n";
            break;

        case BlockType::SetBodyColor:
            code +=
                "            setBodyColor(new Color(" +
                std::to_string(GetRValue(b.bodyColor)) + ", " +
                std::to_string(GetGValue(b.bodyColor)) + ", " +
                std::to_string(GetBValue(b.bodyColor)) +
                "));\n";
            break;

        case BlockType::SetGunColor:
            code +=
                "            setGunColor(new Color(" +
                std::to_string(GetRValue(b.gunColor)) + ", " +
                std::to_string(GetGValue(b.gunColor)) + ", " +
                std::to_string(GetBValue(b.gunColor)) +
                "));\n";
            break;

        case BlockType::SetRadarColor:
            code +=
                "            setRadarColor(new Color(" +
                std::to_string(GetRValue(b.radarColor)) + ", " +
                std::to_string(GetGValue(b.radarColor)) + ", " +
                std::to_string(GetBValue(b.radarColor)) +
                "));\n";
            break;

        case BlockType::SetBulletColor:
            code +=
                "            setBulletColor(new Color(" +
                std::to_string(GetRValue(b.bulletColor)) + ", " +
                std::to_string(GetGValue(b.bulletColor)) + ", " +
                std::to_string(GetBValue(b.bulletColor)) +
                "));\n";
            break;

        case BlockType::SetScanColor:
            code +=
                "            setScanColor(new Color(" +
                std::to_string(GetRValue(b.scanColor)) + ", " +
                std::to_string(GetGValue(b.scanColor)) + ", " +
                std::to_string(GetBValue(b.scanColor)) +
                "));\n";
            break;
        case BlockType::Stop:
            code += "            stop();\n";
            break;

        case BlockType::Resume:
            code += "            resume();\n";
            break;

        case BlockType::TurnGunRight:
            code +=
                "            setTurnGunRight(" +
                std::to_string(b.degrees) + ");\n"
                "            execute();\n";
            break;

        case BlockType::TurnGunLeft:
            code +=
                "            setTurnGunLeft(" +
                std::to_string(b.degrees) + ");\n"
                "            execute();\n";
            break;

        case BlockType::TurnRadarRight:
            code +=
                "            setTurnRadarRight(" +
                std::to_string(b.degrees) + ");\n"
                "            execute();\n";
            break;

        case BlockType::TurnRadarLeft:
            code +=
                "            setTurnRadarLeft(" +
                std::to_string(b.degrees) + ");\n"
                "            execute();\n";
            break;

        case BlockType::SetAdjustGunForRobotTurn:
            code += "            setAdjustGunForRobotTurn(true);\n";
            break;

        case BlockType::SetAdjustRadarForRobotTurn:
            code += "            setAdjustRadarForRobotTurn(true);\n";
            break;

        case BlockType::SetAdjustRadarForGunTurn:
            code += "            setAdjustRadarForGunTurn(true);\n";
            break;

        case BlockType::DoNothing:
            code += "            execute();\n";
            break;

        case BlockType::Wait:
            code +=
                "            waitFor(" +
                std::to_string(static_cast<int>(b.waitSeconds)) +
                ");\n";
            break;
        case BlockType::TurnGunToEnemy:
            code +=
                "            setTurnGunRight(\n"
                "                robocode.util.Utils.normalRelativeAngleDegrees(\n"
                "                    getHeading() + getBearing() - getGunHeading()));\n"
                "            execute();\n";
            break;

        case BlockType::TurnBodyToEnemy:
            code +=
                "            setTurnRight(\n"
                "                robocode.util.Utils.normalRelativeAngleDegrees(\n"
                "                    getHeading() + getBearing() - getHeading()));\n"
                "            execute();\n";
            break;

        case BlockType::GetAwayFromWall:
            code +=
                "            double margin = 100;\n"
                "            if (getX() < margin || getX() > getBattleFieldWidth() - margin ||\n"
                "                getY() < margin || getY() > getBattleFieldHeight() - margin)\n"
                "            {\n"
                "                setBack(150);\n"
                "                setTurnRight(90);\n"
                "                execute();\n"
                "            }\n";
            break;

        case BlockType::HesitateNextBullet:
            code +=
                "            if (getGunHeat() > 0)\n"
                "                execute();\n";
            break;

        case BlockType::KeepAwayRadarFire:
            code +=
                "            setTurnRadarRight(360);\n"
                "            setBack(100);\n"
                "            setFire(1.5);\n"
                "            execute();\n";
            break;

        case BlockType::BodyBlowFire:
            code +=
                "            setAhead(50);\n"
                "            setFire(3);\n"
                "            execute();\n";
            break;

        case BlockType::SmartFire:
            code +=
                "            if (getEnergy() > 50)\n"
                "                setFire(3);\n"
                "            else if (getEnergy() > 20)\n"
                "                setFire(1.5);\n"
                "            else\n"
                "                setFire(0.5);\n"
                "            execute();\n";
            break;
        case BlockType::VictoryDance:
            code +=
                "            setBodyColor(Color.YELLOW);\n"
                "            setGunColor(Color.RED);\n"
                "            setRadarColor(Color.GREEN);\n"
                "            setTurnRight(360);\n"
                "            execute();\n";
            break;

        case BlockType::VictorySpin:
            code +=
                "            setTurnRight(360);\n"
                "            setTurnGunRight(360);\n"
                "            setTurnRadarRight(360);\n"
                "            execute();\n";
            break;
        case BlockType::GetName:
            code += "            getName();\n";
            break;

        case BlockType::GetX:
            code += "            getX();\n";
            break;

        case BlockType::GetY:
            code += "            getY();\n";
            break;

        case BlockType::GetEnergy:
            code += "            getEnergy();\n";
            break;

        case BlockType::GetHeading:
            code += "            getHeading();\n";
            break;

        case BlockType::GetGunHeading:
            code += "            getGunHeading();\n";
            break;

        case BlockType::GetRadarHeading:
            code += "            getRadarHeading();\n";
            break;

        case BlockType::GetGunCoolingRate:
            code += "            getGunCoolingRate();\n";
            break;

        case BlockType::GetGunHeat:
            code += "            getGunHeat();\n";
            break;

        case BlockType::GetVelocity:
            code += "            getVelocity();\n";
            break;

        case BlockType::GetOthers:
            code += "            getOthers();\n";
            break;

        case BlockType::GetBattleFieldHeight:
            code += "            getBattleFieldHeight();\n";
            break;

        case BlockType::GetBattleFieldWidth:
            code += "            getBattleFieldWidth();\n";
            break;

        case BlockType::GetHeight:
            code += "            getHeight();\n";
            break;

        case BlockType::GetWidth:
            code += "            getWidth();\n";
            break;

        case BlockType::GetNumRounds:
            code += "            getNumRounds();\n";
            break;

        case BlockType::GetRoundNum:
            code += "            getRoundNum();\n";
            break;

        case BlockType::GetTime:
            code += "            getTime();\n";
            break;
        }
    }

    code +=
        "        }\n"
        "    }\n"
        "}\n";

    // 5. 正しい finalJavaPath に向かってファイルを書き込む
    std::ofstream file(finalJavaPath.string(), std::ios::binary | std::ios::trunc);
    if (!file.is_open())
    {
        MessageBoxW(nullptr, L"作成に失敗しました！", L"エラー", MB_OK);
        return false;
    }
    file << code;
    file.close();

    MessageBoxW(nullptr, L"ブロックが正常に書き込まれました！", L"デバック", MB_OK);

    // コンパイル用バッチファイルの作成と実行
    const std::string root = "C:\\robocode";
    std::string javac = "C:\\Java\\jdk-17.0.7+7\\bin\\javac.exe";
    std::string logPath = root + "\\javac_log.txt";
    std::string batPath = root + "\\compile_temp.bat";

    {
        std::ofstream bat(batPath);
        bat << "@echo off\nchcp 65001 > nul\n";
        // 1. コンパイルを実行
        bat << "\"" << javac << "\" -encoding UTF-8 -source 8 -target 8 -d \"" << root << "\\robots\" -cp \"" << root << "\\libs\\robocode.jar\" \"" << finalJavaPath.string() << "\" > \"" << logPath << "\" 2>&1\n";

        // 2. 確実に作業ディレクトリを移動してRobocodeを起動する
        bat << "cd /d \"" << root << "\"\n";
        bat << "call robocode.bat\n";

        bat << "exit\n";
    }

    //MessageBoxA(nullptr, "5. Running javac & Robocode via batch...", "Debug", MB_OK);

    // バッチを実行
    int result = std::system(batPath.c_str());

    std::ifstream logFile(logPath);
    std::string output((std::istreambuf_iterator<char>(logFile)), std::istreambuf_iterator<char>());
    if (output.empty()) {
        output = (result == 0) ? "コンパイルが終了しました" : "コンパイルエラーです";
    }

    MessageBoxA(nullptr, output.c_str(), "javac Result", MB_OK);

    return (result == 0);
}