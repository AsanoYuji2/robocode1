/*
#include "JavaGenerator.h"
#include <fstream>
#include <algorithm>
#include <string>

bool ExportJava(const std::vector<Block>& blocks)
{
	std::vector<Block> ordered = blocks;
	std::sort(ordered.begin(), ordered.end(), [](const Block& a, const Block& b) {if (a.x != b.x)return a.x < b.x;return a.y < b.y;});
	std::string code = "import robocode.AdvancedRobot;\n\npublic class BlockRobot extends AdvancedRobot {\n    @Override\n    public void run() {\n";

	for (const auto& b : ordered)
	{
		switch (b.type)
		{
		case BlockType::Move:
			code += "        setAhead(100);\n        execute();\n";
			break;
		case BlockType::Turn:
			code += "        setTurnRight(90);\n        execute();\n";
			break;
		case BlockType::Fire:
			code += "        setFire(2);\n        execute();\n";
			break;
		}
	}
	code += "    }\n}\n";
	std::ofstream file("C:\\robocode\\robots\\BlockRobot.java");
	// std::ofstream file("BlockRobot.java");
	if (!file.is_open())
		return false;
	file << code;
	return file.good();
}
*/

#include "JavaGenerator.h"
#include <fstream>
#include <algorithm>
#include <string>
#include <filesystem>
#include <cstdlib>

bool ExportJava(const std::vector<Block>& blocks)
{
	std::vector<Block> ordered = blocks;
	std::sort(ordered.begin(), ordered.end(), [](const Block& a, const Block& b) {if (a.x != b.x)return a.x < b.x;return a.y < b.y;});

	std::string code =
		"package blockrobot;\n\n"
		"import robocode.AdvancedRobot;\n\n"
		"public class BlockRobot extends AdvancedRobot \n{\n"
		"    @Override\n"
		"	public void run() \n	{\n"
		"		while (true) \n		{\n";

	for (const auto& b : ordered)
	{
		switch (b.type)
		{
		case BlockType::Move:      code += "        setAhead(100);\n        execute();\n";
			break;
		case BlockType::Bark:      code += "        back(100);\n			execute();\n";
			break;
		case BlockType::Turn:      code += "        turnRight(90);\n		execute();\n";
			break;
		case BlockType::TurnLeft:  code += "            turnLeft(90);\n"; break;
		case BlockType::MoveRight: code += "            turnRight(90);\n            ahead(100);\n            turnLeft(90);\n"; break;
		case BlockType::MoveLeft:  code += "            turnLeft(90);\n            ahead(100);\n            turnRight(90);\n"; break;
		case BlockType::Fire:      code += "            fire(2);\n"; break;
		}
	}
	code += "        }\n    }\n}\n";

	// 出力先(自分の環境に合わせて変更)
	const std::string root = "C:\\robocode";
	const std::string dir = root + "\\robots\\blockrobot"; //保存先のファイル
	std::filesystem::create_directories(dir);

	const std::string javaPath = dir + "\\BlockRobot.java";
	{
		std::ofstream file(javaPath, std::ios::binary);
		if (!file.is_open())
			return false;
		file << code;
		if (!file.good())
			return false;
	}


	// javacでコンパイル(JDKのPATHが必要)
	const std::string log = root + "\\javac_log.txt";
	std::string javac = "C:\\インターン\\jdk-17.0.7+7\\bin\\javac.exe"; // ← 見つかったパスに変更
	std::string cmd = "\"\"" + javac + "\" -encoding UTF-8 -cp \"" + root + "\\libs\\robocode.jar\" -d \"" +
		root + "\\robots\" \"" + javaPath + "\" > \"" + log + "\" 2>&1\"";
	return std::system(cmd.c_str()) == 0;
}