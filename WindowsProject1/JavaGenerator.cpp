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

bool ExportJava(const std::vector<Block>& blocks)
{
    MessageBoxA(nullptr, "ExportJava called!", "Debug", MB_OK);

    std::wstring savePath = SelectSavePath();

    if (savePath.empty())
        return false;

    std::filesystem::path path(savePath);

    std::filesystem::create_directories(
        path.parent_path()
    );

    std::string packageName = path.parent_path().filename().string();
    std::string className = path.stem().string();

    MessageBoxA(nullptr, "3. Class & Package obtained!", "Debug", MB_OK);

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

    // 1. パッケージ宣言
    if (!packageName.empty() && packageName != "robots")
    {
        code += "package " + packageName + ";\n\n";
    }

    // 2. インポート文（必ずクラス定義の前に置く）
    code +=
        "import robocode.*;\n"
        "import java.awt.Color;\n"
        "import static robocode.util.Utils.normalRelativeAngleDegrees;\n\n";

    // 3. クラス定義
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
                "            setAhead(100);\n"
                "            execute();\n";
            break;

        case BlockType::Bark:
            code +=
                "            setBack(100);\n"
                "            execute();\n";
            break;

        case BlockType::Turn:
            code +=
                "            setTurnRight(90);\n"
                "            execute();\n";
            break;

        case BlockType::TurnLeft:
            code +=
                "            setTurnLeft(90);\n"
                "            execute();\n";
            break;

        case BlockType::MoveRight:
            code +=
                "            turnRight(90);\n"
                "            ahead(100);\n"
                "            turnLeft(90);\n"
                "            execute();\n";
            break;

        case BlockType::MoveLeft:
            code +=
                "            turnLeft(90);\n"
                "            ahead(100);\n"
                "            turnRight(90);\n"
                "            execute();\n";
            break;

        case BlockType::Fire:
            code +=
                "            fire(2);\n"
                "            execute();\n";
            break;
        }
    }

    code +=
        "        }\n"
        "    }\n"
        "}\n";

    std::ofstream file(path.string(), std::ios::binary | std::ios::trunc);
    if (!file.is_open())
    {
        MessageBoxA(nullptr, "Error: Failed to open file for writing!", "Debug", MB_OK);
        return false;
    }
    file << code;
    file.close();

    MessageBoxA(nullptr, "4. Java file written successfully!", "Debug", MB_OK);

    // コンパイル用バッチファイルの作成と実行
    const std::string root = "C:\\robocode";
    std::string javac = "C:\\インターン\\jdk-17.0.7+7\\bin\\javac.exe";
    std::string logPath = root + "\\javac_log.txt";
    std::string batPath = root + "\\compile_temp.bat";

    {
        std::ofstream bat(batPath);
        bat << "@echo off\n";
        bat << "\"" << javac << "\" -encoding UTF-8 -d \"" << root << "\\robots\" -cp \"" << root << "\\libs\\robocode.jar\" \"" << path.string() << "\" > \"" << logPath << "\" 2>&1\n";
        bat << "exit\n"; // ← ここに exit を追加して自動終了させます
    }

    MessageBoxA(nullptr, "5. Running javac via batch...", "Debug", MB_OK);

    int result = std::system(batPath.c_str());

    // ログファイルを読み込んでポップアップ表示
    std::ifstream logFile(logPath);
    std::string output((std::istreambuf_iterator<char>(logFile)), std::istreambuf_iterator<char>());
    if (output.empty()) {
        output = (result == 0) ? "Compile finished successfully!" : "Compilation failed with unknown error.";
    }

    MessageBoxA(nullptr, output.c_str(), "javac Result", MB_OK);

    return (result == 0);
}