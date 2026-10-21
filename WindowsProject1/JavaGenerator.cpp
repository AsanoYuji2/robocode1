#include <windows.h>
#include <commdlg.h>
#include <fstream>
#include <algorithm>
#include <string>
#include <filesystem>
#include <cstdlib>

#include "JavaGenerator.h"

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

bool ExportJavaCustom(const std::vector<Block>& blocks, const std::wstring& targetFolder, const std::wstring& robotName)
{
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

    std::vector<Block> ordered = blocks;

    std::sort(
        ordered.begin(),
        ordered.end(),
        [](const Block& a, const Block& b)
        {
            if (a.x != b.x)
                return a.x < b.x;

            return a.y < b.y;
        }
    );

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