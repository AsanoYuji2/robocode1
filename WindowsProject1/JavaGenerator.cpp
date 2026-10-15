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
    std::wstring savePath = SelectSavePath();

    if (savePath.empty())
        return false;

    std::filesystem::path path(savePath);

    std::filesystem::create_directories(
        path.parent_path()
    );

    std::string className =
        path.stem().string();

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

    std::string code =
        "import robocode.AdvancedRobot;\n\n"
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

    std::ofstream file(path.string(), std::ios::binary);

    if (!file.is_open())
        return false;

    file << code;
    file.close();

    const std::string root = "C:\\robocode";
    std::string javac = "C:\\ƒCƒ“ƒ^[ƒ“\\jdk-17.0.7+7\\bin\\javac.exe";
    std::string log = root + "\\javac_log.txt";
    std::string cmd = "cmd /c \"\"" + javac + "\" -encoding UTF-8 -cp \"" +
        root + "\\libs\\robocode.jar\" \"" + path.string() +"\"\"";

    int result = std::system(cmd.c_str());
    return result == 0; 
}