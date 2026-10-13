#include "JavaGenerator.h"
#include <fstream>
#include <algorithm>
#include <string>
bool ExportJava(const std::vector<Block>& blocks)
{
	std::vector<Block> ordered = blocks;
	std::sort(ordered.begin(), ordered.end(), [](const Block& a, const Block& b) {if (a.x != b.x)return a.x < b.x;return a.y < b.y;});
	std::string code = "import robocode.AdvancedRobot;\\n\\npublic class BlockRobot extends AdvancedRobot {\\n    @Override\\n    public void run() {\\n";
	for (const auto& b : ordered) switch (b.type) {
	case BlockType::Move: code += "        setAhead(100);\\n        execute();\\n"; break;
	case BlockType::Turn: code += "        setTurnRight(90);\\n        execute();\\n"; break;
	case BlockType::Fire: code += "        setFire(2);\\n        execute();\\n"; break;
	}
	code += "    }\\n}\\n";
	std::ofstream file("BlockRobot.java"); if (!file.is_open()) return false; file << code; return file.good();
}
