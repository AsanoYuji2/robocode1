#include <windows.h>
#include <commdlg.h>
#include <fstream>
#include <algorithm>
#include <string>
#include <filesystem>
#include <cstdlib>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <map>
#include <set>
#include <vector>
#include <limits>
#include <cctype>

#include "JavaGenerator.h"
#include "UserFunction.h"

namespace
{
    struct ExpandedBlock
    {
        Block block{};
        bool syntheticRepeatEnd = false;
    };

    struct JavaIdentifiers
    {
        std::map<std::wstring, std::string> numeric;
        std::map<std::wstring, std::string> boolean;
    };

    struct OpenStructure
    {
        enum class Kind { Loop, If } kind = Kind::If;
        bool hasElse = false;
        std::string loopName;
    };

    std::string WideToUtf8(const std::wstring& value)
    {
        if (value.empty()) return std::string();
        const int required = WideCharToMultiByte(
            CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()),
            nullptr, 0, nullptr, nullptr);
        if (required <= 0) return std::string();
        std::string result(static_cast<size_t>(required), '\0');
        WideCharToMultiByte(
            CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()),
            &result[0], required, nullptr, nullptr);
        return result;
    }

    std::string JavaNumber(double value)
    {
        if (!std::isfinite(value)) value = 0.0;
        std::ostringstream stream;
        stream << std::setprecision(15) << value;
        std::string text = stream.str();
        if (text.find('.') == std::string::npos &&
            text.find('e') == std::string::npos &&
            text.find('E') == std::string::npos)
        {
            text += ".0";
        }
        return text;
    }

    std::string JavaIndent(int level)
    {
        return std::string(static_cast<size_t>((std::max)(0, level) * 4), ' ');
    }

    void AddName(std::set<std::wstring>& names, const std::wstring& name, const wchar_t* fallback)
    {
        names.insert(name.empty() ? fallback : name);
    }

    void CollectSimpleConditionNames(
        ConditionType condition,
        const std::wstring& variableName,
        const std::wstring& booleanVariableName,
        std::set<std::wstring>& numericNames,
        std::set<std::wstring>& booleanNames)
    {
        switch (condition)
        {
        case ConditionType::VariableAtLeast:
        case ConditionType::VariableAtMost:
        case ConditionType::VariableEqual:
        case ConditionType::VariableNotEqual:
        case ConditionType::VariableGreaterThan:
        case ConditionType::VariableLessThan:
            AddName(numericNames, variableName, L"score");
            break;
        case ConditionType::BooleanVariable:
            AddName(booleanNames, booleanVariableName, L"flag");
            break;
        default:
            break;
        }
    }

    void CollectVariableNames(
        const std::vector<ExpandedBlock>& blocks,
        std::set<std::wstring>& numericNames,
        std::set<std::wstring>& booleanNames)
    {
        for (const ExpandedBlock& expanded : blocks)
        {
            const Block& b = expanded.block;
            switch (b.type)
            {
            case BlockType::CreateVariable:
            case BlockType::SetVariable:
            case BlockType::ChangeVariable:
            case BlockType::AddVariable:
            case BlockType::SubtractVariable:
                AddName(numericNames, b.variableName, L"score");
                break;
            default:
                break;
            }

            if (b.type == BlockType::SetBoolean)
                AddName(booleanNames, b.booleanVariableName, L"flag");

            if (b.type == BlockType::If)
            {
                if (b.condition == ConditionType::And || b.condition == ConditionType::Or)
                {
                    CollectSimpleConditionNames(
                        b.leftCondition, b.leftVariableName, b.booleanVariableName,
                        numericNames, booleanNames);
                    CollectSimpleConditionNames(
                        b.rightCondition, b.rightVariableName, b.booleanVariableName,
                        numericNames, booleanNames);
                }
                else if (b.condition == ConditionType::Not)
                {
                    CollectSimpleConditionNames(
                        b.leftCondition, b.leftVariableName, b.booleanVariableName,
                        numericNames, booleanNames);
                }
                else
                {
                    CollectSimpleConditionNames(
                        b.condition, b.variableName, b.booleanVariableName,
                        numericNames, booleanNames);
                }
            }
        }
    }

    std::string MakeSafeSuffix(const std::wstring& source)
    {
        std::string result;
        for (wchar_t ch : source)
        {
            if ((ch >= L'a' && ch <= L'z') ||
                (ch >= L'A' && ch <= L'Z') ||
                (ch >= L'0' && ch <= L'9') || ch == L'_')
            {
                result.push_back(static_cast<char>(ch));
            }
            else
            {
                result.push_back('_');
            }
        }
        if (result.empty()) result = "value";
        if (result[0] >= '0' && result[0] <= '9') result = "v_" + result;
        return result;
    }

    void BuildIdentifierMap(
        const std::set<std::wstring>& names,
        const std::string& prefix,
        std::map<std::wstring, std::string>& output,
        std::set<std::string>& allUsed)
    {
        for (const std::wstring& rawName : names)
        {
            std::string base = prefix + MakeSafeSuffix(rawName);
            std::string candidate = base;
            int suffix = 2;
            while (allUsed.find(candidate) != allUsed.end())
            {
                candidate = base + "_" + std::to_string(suffix++);
            }
            allUsed.insert(candidate);
            output[rawName] = candidate;
        }
    }

    JavaIdentifiers BuildJavaIdentifiers(const std::vector<ExpandedBlock>& blocks)
    {
        std::set<std::wstring> numericNames;
        std::set<std::wstring> booleanNames;
        CollectVariableNames(blocks, numericNames, booleanNames);

        JavaIdentifiers ids;
        std::set<std::string> used;
        BuildIdentifierMap(numericNames, "num_", ids.numeric, used);
        BuildIdentifierMap(booleanNames, "bool_", ids.boolean, used);
        return ids;
    }

    std::string NumericId(const JavaIdentifiers& ids, const std::wstring& name)
    {
        const std::wstring actual = name.empty() ? L"score" : name;
        const auto it = ids.numeric.find(actual);
        return it == ids.numeric.end() ? "num_score" : it->second;
    }

    std::string BooleanId(const JavaIdentifiers& ids, const std::wstring& name)
    {
        const std::wstring actual = name.empty() ? L"flag" : name;
        const auto it = ids.boolean.find(actual);
        return it == ids.boolean.end() ? "bool_flag" : it->second;
    }

    std::string SimpleConditionExpression(
        ConditionType condition,
        const std::wstring& variableName,
        int value,
        const std::wstring& booleanVariableName,
        const JavaIdentifiers& ids)
    {
        const std::string number = std::to_string(value);
        const std::string variable = NumericId(ids, variableName);
        const std::string booleanVariable = BooleanId(ids, booleanVariableName);

        switch (condition)
        {
        case ConditionType::Always:
        case ConditionType::TrueValue:
            return "true";
        case ConditionType::FalseValue:
            return "false";
        case ConditionType::HealthAtLeast:
            return "getEnergy() >= " + number;
        case ConditionType::VariableAtLeast:
            return variable + " >= " + number;
        case ConditionType::VariableAtMost:
            return variable + " <= " + number;
        case ConditionType::VariableEqual:
            return variable + " == " + number;
        case ConditionType::VariableNotEqual:
            return variable + " != " + number;
        case ConditionType::VariableGreaterThan:
            return variable + " > " + number;
        case ConditionType::VariableLessThan:
            return variable + " < " + number;
        case ConditionType::EnemyDetected:
            return "hasRecentTarget()";
        case ConditionType::EnemyDistanceAtMost:
            return "(hasRecentTarget() && lastScannedDistance <= " + number + ")";
        case ConditionType::EnemyOnLeft:
            return "(hasRecentTarget() && getTargetRelativeBearing() < 0.0)";
        case ConditionType::EnemyOnRight:
            return "(hasRecentTarget() && getTargetRelativeBearing() > 0.0)";
        case ConditionType::EnemyAhead:
            return "(hasRecentTarget() && Math.abs(getTargetRelativeBearing()) <= 45.0)";
        case ConditionType::EnemyBehind:
            return "(hasRecentTarget() && Math.abs(getTargetRelativeBearing()) >= 135.0)";
        case ConditionType::WallNear:
            return "(Math.min(Math.min(getX(), getBattleFieldWidth() - getX()), "
                "Math.min(getY(), getBattleFieldHeight() - getY())) <= " + number + ")";
        case ConditionType::EnergyAtMost:
            return "getEnergy() <= " + number;
        case ConditionType::BooleanVariable:
            return booleanVariable;
        case ConditionType::And:
        case ConditionType::Or:
        case ConditionType::Not:
            return "false";
        }
        return "false";
    }

    std::string BlockConditionExpression(const Block& b, const JavaIdentifiers& ids)
    {
        if (b.condition == ConditionType::And)
        {
            return "(" + SimpleConditionExpression(
                b.leftCondition, b.leftVariableName, b.leftConditionValue,
                b.booleanVariableName, ids) + " && " + SimpleConditionExpression(
                    b.rightCondition, b.rightVariableName, b.rightConditionValue,
                    b.booleanVariableName, ids) + ")";
        }
        if (b.condition == ConditionType::Or)
        {
            return "(" + SimpleConditionExpression(
                b.leftCondition, b.leftVariableName, b.leftConditionValue,
                b.booleanVariableName, ids) + " || " + SimpleConditionExpression(
                    b.rightCondition, b.rightVariableName, b.rightConditionValue,
                    b.booleanVariableName, ids) + ")";
        }
        if (b.condition == ConditionType::Not)
        {
            return "(!(" + SimpleConditionExpression(
                b.leftCondition, b.leftVariableName, b.leftConditionValue,
                b.booleanVariableName, ids) + "))";
        }
        return SimpleConditionExpression(
            b.condition, b.variableName, b.conditionValue,
            b.booleanVariableName, ids);
    }

    bool IsJavaIdentifier(const std::wstring& value)
    {
        if (value.empty()) return false;
        auto isStart = [](wchar_t ch)
            {
                return (ch >= L'a' && ch <= L'z') ||
                    (ch >= L'A' && ch <= L'Z') ||
                    ch == L'_' || ch == L'$';
            };
        auto isPart = [&](wchar_t ch)
            {
                return isStart(ch) || (ch >= L'0' && ch <= L'9');
            };
        if (!isStart(value.front())) return false;
        for (wchar_t ch : value)
            if (!isPart(ch)) return false;

        static const std::set<std::wstring> keywords =
        {
            L"abstract", L"assert", L"boolean", L"break", L"byte", L"case",
            L"catch", L"char", L"class", L"const", L"continue", L"default",
            L"do", L"double", L"else", L"enum", L"extends", L"final",
            L"finally", L"float", L"for", L"goto", L"if", L"implements",
            L"import", L"instanceof", L"int", L"interface", L"long", L"native",
            L"new", L"package", L"private", L"protected", L"public", L"return",
            L"short", L"static", L"strictfp", L"super", L"switch", L"synchronized",
            L"this", L"throw", L"throws", L"transient", L"try", L"void",
            L"volatile", L"while", L"true", L"false", L"null"
        };
        return keywords.find(value) == keywords.end();
    }

    bool ExpandGraphAndCalls(
        const std::vector<Block>& blocks,
        const std::vector<int>& parents,
        const std::vector<int>& repeatBodies,
        const std::vector<UserDefinedFunction>& functions,
        std::vector<std::wstring>& callStack,
        std::vector<ExpandedBlock>& output,
        std::wstring& error,
        int depth);

    bool ExpandSingleBlockOrCall(
        const Block& block,
        const std::vector<UserDefinedFunction>& functions,
        std::vector<std::wstring>& callStack,
        std::vector<ExpandedBlock>& output,
        std::wstring& error,
        int depth)
    {
        if (!IsUserFunctionCall(block))
        {
            output.push_back({ block, false });
            return true;
        }

        const std::wstring name = GetUserFunctionCallName(block);
        const auto found = std::find_if(
            functions.begin(), functions.end(),
            [&](const UserDefinedFunction& function)
            {
                return function.name == name;
            });

        if (found == functions.end())
        {
            error = L"呼び出し先の自作関数「" + name + L"」が見つかりません。";
            return false;
        }

        if (std::find(callStack.begin(), callStack.end(), name) != callStack.end())
        {
            error = L"自作関数の呼び出しが循環しています。";
            return false;
        }

        if (depth >= 32)
        {
            error = L"自作関数の呼び出しが深すぎます。関数の入れ子は32段までです。";
            return false;
        }

        callStack.push_back(name);
        const bool success = ExpandGraphAndCalls(
            found->blocks, found->parents, found->repeatBodies,
            functions, callStack, output, error, depth + 1);
        callStack.pop_back();
        return success;
    }

    bool ExpandGraphAndCalls(
        const std::vector<Block>& blocks,
        const std::vector<int>& parents,
        const std::vector<int>& repeatBodies,
        const std::vector<UserDefinedFunction>& functions,
        std::vector<std::wstring>& callStack,
        std::vector<ExpandedBlock>& output,
        std::wstring& error,
        int depth)
    {
        if (blocks.empty()) return true;
        if (depth > 32)
        {
            error = L"自作関数の呼び出しが深すぎます。";
            return false;
        }

        const size_t count = blocks.size();
        const bool hasParentGraph = parents.size() == count;
        std::vector<int> safeParents(count, -1);
        if (hasParentGraph) safeParents = parents;

        std::vector<int> safeRepeatBodies(count, -1);
        if (repeatBodies.size() == count) safeRepeatBodies = repeatBodies;

        if (!hasParentGraph)
        {
            std::vector<size_t> order(count);
            for (size_t i = 0; i < count; ++i) order[i] = i;
            std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b)
                {
                    if (blocks[a].y != blocks[b].y) return blocks[a].y < blocks[b].y;
                    if (blocks[a].x != blocks[b].x) return blocks[a].x < blocks[b].x;
                    return a < b;
                });
            for (size_t index : order)
            {
                if (!ExpandSingleBlockOrCall(
                    blocks[index], functions, callStack, output, error, depth))
                    return false;
            }
            return true;
        }

        std::vector<int> childOf(count, -1);
        for (size_t i = 0; i < count; ++i)
        {
            const int parent = safeParents[i];
            if (parent < 0 || parent >= static_cast<int>(count) || parent == static_cast<int>(i))
                continue;

            int& existing = childOf[static_cast<size_t>(parent)];
            if (existing < 0 || blocks[i].y < blocks[static_cast<size_t>(existing)].y)
                existing = static_cast<int>(i);
        }

        std::vector<bool> isRepeatBodyRoot(count, false);
        for (size_t i = 0; i < count; ++i)
        {
            const int body = safeRepeatBodies[i];
            if (blocks[i].type == BlockType::Repeat &&
                body >= 0 && body < static_cast<int>(count))
                isRepeatBodyRoot[static_cast<size_t>(body)] = true;
        }

        std::vector<bool> visited(count, false);
        std::function<bool(int)> appendChain;
        appendChain = [&](int start) -> bool
            {
                int current = start;
                for (size_t step = 0;
                    current >= 0 && current < static_cast<int>(count) && step < count;
                    ++step)
                {
                    if (visited[static_cast<size_t>(current)]) break;
                    visited[static_cast<size_t>(current)] = true;
                    const Block& block = blocks[static_cast<size_t>(current)];

                    if (IsUserFunctionCall(block))
                    {
                        if (!ExpandSingleBlockOrCall(
                            block, functions, callStack, output, error, depth))
                            return false;
                    }
                    else
                    {
                        output.push_back({ block, false });
                        const int body = safeRepeatBodies[static_cast<size_t>(current)];
                        if (block.type == BlockType::Repeat &&
                            body >= 0 && body < static_cast<int>(count))
                        {
                            if (!appendChain(body)) return false;
                            Block endBlock{};
                            endBlock.type = BlockType::EndRepeat;
                            output.push_back({ endBlock, true });
                        }
                    }

                    current = childOf[static_cast<size_t>(current)];
                }
                return true;
            };

        std::vector<int> roots;
        for (size_t i = 0; i < count; ++i)
        {
            if (safeParents[i] == -1 && !isRepeatBodyRoot[i])
                roots.push_back(static_cast<int>(i));
        }
        std::stable_sort(roots.begin(), roots.end(), [&](int a, int b)
            {
                if (blocks[static_cast<size_t>(a)].y != blocks[static_cast<size_t>(b)].y)
                    return blocks[static_cast<size_t>(a)].y < blocks[static_cast<size_t>(b)].y;
                if (blocks[static_cast<size_t>(a)].x != blocks[static_cast<size_t>(b)].x)
                    return blocks[static_cast<size_t>(a)].x < blocks[static_cast<size_t>(b)].x;
                return a < b;
            });

        for (int root : roots)
            if (!appendChain(root)) return false;

        std::vector<int> leftovers;
        for (size_t i = 0; i < count; ++i)
            if (!visited[i]) leftovers.push_back(static_cast<int>(i));
        std::stable_sort(leftovers.begin(), leftovers.end(), [&](int a, int b)
            {
                if (blocks[static_cast<size_t>(a)].y != blocks[static_cast<size_t>(b)].y)
                    return blocks[static_cast<size_t>(a)].y < blocks[static_cast<size_t>(b)].y;
                if (blocks[static_cast<size_t>(a)].x != blocks[static_cast<size_t>(b)].x)
                    return blocks[static_cast<size_t>(a)].x < blocks[static_cast<size_t>(b)].x;
                return a < b;
            });
        for (int item : leftovers)
            if (!appendChain(item)) return false;

        return true;
    }

    void EmitLine(std::string& code, int indent, const std::string& line)
    {
        code += JavaIndent(indent) + line + "\n";
    }

    void EmitVariableFields(std::string& code, const JavaIdentifiers& ids)
    {
        for (const auto& item : ids.numeric)
            EmitLine(code, 1, "private int " + item.second + " = 0;");
        for (const auto& item : ids.boolean)
            EmitLine(code, 1, "private boolean " + item.second + " = false;");
        if (!ids.numeric.empty() || !ids.boolean.empty())
            code += "\n";
    }

    void EmitOrdinaryBlock(
        const ExpandedBlock& expanded,
        const JavaIdentifiers& ids,
        std::string& code,
        int indent,
        int& repeatIndex)
    {
        const Block& b = expanded.block;
        const std::string distance = std::to_string(b.distance);
        const std::string degrees = JavaNumber(b.degrees);
        const std::string power = JavaNumber((std::max)(0.1, (std::min)(3.0, b.power)));
        const std::string variable = NumericId(ids, b.variableName);
        const std::string booleanVariable = BooleanId(ids, b.booleanVariableName);

        switch (b.type)
        {
        case BlockType::Move:
            EmitLine(code, indent, "ahead(" + distance + ");");
            break;
        case BlockType::MoveBackward:
            EmitLine(code, indent, "back(" + distance + ");");
            break;
        case BlockType::MoveRight:
            EmitLine(code, indent, "turnRight(90);");
            EmitLine(code, indent, "ahead(" + distance + ");");
            EmitLine(code, indent, "turnLeft(90);");
            break;
        case BlockType::MoveLeft:
            EmitLine(code, indent, "turnLeft(90);");
            EmitLine(code, indent, "ahead(" + distance + ");");
            EmitLine(code, indent, "turnRight(90);");
            break;
        case BlockType::TurnRight:
            EmitLine(code, indent, "turnRight(" + degrees + ");");
            break;
        case BlockType::TurnLeft:
            EmitLine(code, indent, "turnLeft(" + degrees + ");");
            break;
        case BlockType::Stop:
            EmitLine(code, indent, "stop();");
            break;
        case BlockType::Resume:
            EmitLine(code, indent, "resume();");
            break;
        case BlockType::TurnGunRight:
            EmitLine(code, indent, "turnGunRight(" + degrees + ");");
            break;
        case BlockType::TurnGunLeft:
            EmitLine(code, indent, "turnGunLeft(" + degrees + ");");
            break;
        case BlockType::TurnRadarRight:
            EmitLine(code, indent, "turnRadarRight(" + degrees + ");");
            break;
        case BlockType::TurnRadarLeft:
            EmitLine(code, indent, "turnRadarLeft(" + degrees + ");");
            break;
        case BlockType::SetAdjustGunForRobotTurn:
            EmitLine(code, indent, "setAdjustGunForRobotTurn(true);");
            break;
        case BlockType::SetAdjustRadarForRobotTurn:
            EmitLine(code, indent, "setAdjustRadarForRobotTurn(true);");
            break;
        case BlockType::SetAdjustRadarForGunTurn:
            EmitLine(code, indent, "setAdjustRadarForGunTurn(true);");
            break;
        case BlockType::FireBullet:
            EmitLine(code, indent, "fire(" + power + ");");
            break;
        case BlockType::FireBulletAndGetResult:
            EmitLine(code, indent, "fire(" + power + ");");
            break;
        case BlockType::DoNothing:
            EmitLine(code, indent, "execute();");
            break;
        case BlockType::Wait:
        {
            double seconds = std::isfinite(b.waitSeconds) ? b.waitSeconds : 0.0;
            seconds = (std::max)(0.0, (std::min)(seconds, 3600.0));
            const long long turns = static_cast<long long>(std::ceil(seconds * 30.0));
            EmitLine(code, indent, "for (int i = 0; i < " + std::to_string(turns) + "; i++) { execute(); }");
            break;
        }
        case BlockType::Repeat:
        {
            const std::string loopName = "__repeatIndex" + std::to_string(repeatIndex++);
            const int count = (std::max)(0, (std::min)(b.repeatCount, 1000000));
            EmitLine(code, indent, "for (int " + loopName + " = 0; " + loopName +
                " < " + std::to_string(count) + "; ++" + loopName + ") {");
            break;
        }
        case BlockType::EndRepeat:
            break;
        case BlockType::If:
            EmitLine(code, indent, "if (" + BlockConditionExpression(b, ids) + ") {");
            break;
        case BlockType::Else:
        case BlockType::EndIf:
            break;

        case BlockType::CreateVariable:
        case BlockType::SetVariable:
            EmitLine(code, indent, variable + " = " + std::to_string(b.value) + ";");
            break;
        case BlockType::ChangeVariable:
        case BlockType::AddVariable:
            EmitLine(code, indent, variable + " += " + std::to_string(b.value) + ";");
            break;
        case BlockType::SubtractVariable:
            EmitLine(code, indent, variable + " -= " + std::to_string(b.value) + ";");
            break;
        case BlockType::SetBoolean:
            EmitLine(code, indent, booleanVariable + " = " + (b.booleanValue ? "true;" : "false;"));
            break;

            // イベント用関数を各ハンドラ内で安全に引数つきで呼び出す
        case BlockType::KeepAwayRadarFire:
            EmitLine(code, indent, "keepAwayRadarFire(e);");
            break;
        case BlockType::BodyBlowFire:
            EmitLine(code, indent, "bodyBlowFire(e);");
            break;
        case BlockType::SmartFire:
            EmitLine(code, indent, "smartFire(e);");
            break;
        case BlockType::GetAwayFromWall:
            EmitLine(code, indent, "getAwayFromWall(e);");
            break;
        case BlockType::TurnBodyToEnemy:
            EmitLine(code, indent, "turnBodyToEnemy(e);");
            break;
        case BlockType::TurnGunToEnemy:
            EmitLine(code, indent, "turnGunToEnemy(e);");
            break;
        case BlockType::HesitateNextBullet:
            EmitLine(code, indent, "hesitateNextBullet(e);");
            break;

        case BlockType::OnScannedRobot:
        case BlockType::OnHitWall:
        case BlockType::OnBulletHit:
        case BlockType::OnHitByBullet:
        case BlockType::OnHitRobot:
        case BlockType::OnWin:
            break;

        case BlockType::VictoryDance:
            EmitLine(code, indent, "victoryDance();");
            break;
        case BlockType::VictorySpin:
            EmitLine(code, indent, "victorySpin();");
            break;

        case BlockType::SetBodyColor:
            EmitLine(code, indent, "setBodyColor(new Color(" +
                std::to_string(GetRValue(b.bodyColor)) + ", " +
                std::to_string(GetGValue(b.bodyColor)) + ", " +
                std::to_string(GetBValue(b.bodyColor)) + "));");
            break;
        case BlockType::SetGunColor:
            EmitLine(code, indent, "setGunColor(new Color(" +
                std::to_string(GetRValue(b.gunColor)) + ", " +
                std::to_string(GetGValue(b.gunColor)) + ", " +
                std::to_string(GetBValue(b.gunColor)) + "));");
            break;
        case BlockType::SetRadarColor:
            EmitLine(code, indent, "setRadarColor(new Color(" +
                std::to_string(GetRValue(b.radarColor)) + ", " +
                std::to_string(GetGValue(b.radarColor)) + ", " +
                std::to_string(GetBValue(b.radarColor)) + "));");
            break;
        case BlockType::SetBulletColor:
            EmitLine(code, indent, "setBulletColor(new Color(" +
                std::to_string(GetRValue(b.bulletColor)) + ", " +
                std::to_string(GetGValue(b.bulletColor)) + ", " +
                std::to_string(GetBValue(b.bulletColor)) + "));");
            break;
        case BlockType::SetScanColor:
            EmitLine(code, indent, "setScanColor(new Color(" +
                std::to_string(GetRValue(b.scanColor)) + ", " +
                std::to_string(GetGValue(b.scanColor)) + ", " +
                std::to_string(GetBValue(b.scanColor)) + "));");
            break;

        case BlockType::Scan:
            EmitLine(code, indent, "scan();");
            break;
        case BlockType::ForwardBackwardGun360:
            EmitLine(code, indent, "aheadAndBack();");
            break;
        case BlockType::ZigzagMove:
            EmitLine(code, indent, "limpRunning();");
            break;
        }
    }

    void EmitBlockSequence(
        const std::vector<ExpandedBlock>& blocks,
        const JavaIdentifiers& ids,
        std::string& code,
        int initialIndent)
    {
        int indent = initialIndent;
        int repeatIndex = 0;
        std::vector<OpenStructure> structures;

        for (const ExpandedBlock& expanded : blocks)
        {
            const Block& b = expanded.block;

            if (b.type == BlockType::Repeat)
            {
                EmitOrdinaryBlock(expanded, ids, code, indent, repeatIndex);
                OpenStructure open;
                open.kind = OpenStructure::Kind::Loop;
                open.loopName = "__repeatIndex" + std::to_string(repeatIndex - 1);
                structures.push_back(open);
                ++indent;
                continue;
            }

            if (b.type == BlockType::If)
            {
                EmitOrdinaryBlock(expanded, ids, code, indent, repeatIndex);
                OpenStructure open;
                open.kind = OpenStructure::Kind::If;
                structures.push_back(open);
                ++indent;
                continue;
            }

            if (b.type == BlockType::Else)
            {
                if (!structures.empty() &&
                    structures.back().kind == OpenStructure::Kind::If &&
                    !structures.back().hasElse)
                {
                    --indent;
                    EmitLine(code, indent, "} else {");
                    ++indent;
                    structures.back().hasElse = true;
                }
                continue;
            }

            if (b.type == BlockType::EndIf)
            {
                if (!structures.empty() && structures.back().kind == OpenStructure::Kind::If)
                {
                    --indent;
                    EmitLine(code, indent, "}");
                    structures.pop_back();
                }
                continue;
            }

            if (b.type == BlockType::EndRepeat)
            {
                if (!structures.empty() && structures.back().kind == OpenStructure::Kind::Loop)
                {
                    --indent;
                    EmitLine(code, indent, "}");
                    structures.pop_back();
                }
                continue;
            }

            EmitOrdinaryBlock(expanded, ids, code, indent, repeatIndex);
        }

        while (!structures.empty())
        {
            --indent;
            EmitLine(code, indent, "}");
            structures.pop_back();
        }
    }

    // Sample.java の標準関数定義を出力するヘルパー
    void EmitHelperMethods(std::string& code)
    {
        code +=
            "    private void aheadAndBack() {\n"
            "        ahead(100);\n"
            "        turnGunRight(360);\n"
            "        back(100);\n"
            "        turnGunRight(360);\n"
            "    }\n\n"

            "    private void limpRunning() {\n"
            "        setAhead(40000);\n"
            "        setTurnRight(90);\n"
            "        waitFor(new TurnCompleteCondition(this));\n"
            "        setTurnLeft(180);\n"
            "        waitFor(new TurnCompleteCondition(this));\n"
            "        setTurnRight(180);\n"
            "        waitFor(new TurnCompleteCondition(this));\n"
            "    }\n\n"

            "    private void spinRunning() {\n"
            "        setTurnRight(10000);\n"
            "        setMaxVelocity(5);\n"
            "        ahead(1000);\n"
            "    }\n\n"

            "    private void keepAwayRadarFire(ScannedRobotEvent e) {\n"
            "        double absoluteBearing = getHeading() + e.getBearing();\n"
            "        double bearingFromGun = normalRelativeAngleDegrees(absoluteBearing - getGunHeading());\n"
            "        if (Math.abs(bearingFromGun) <= 3) {\n"
            "            turnGunRight(bearingFromGun);\n"
            "            if (getGunHeat() == 0) {\n"
            "                fire(Math.min(3 - Math.abs(bearingFromGun), getEnergy() - .1));\n"
            "            }\n"
            "        } else {\n"
            "            turnGunRight(bearingFromGun);\n"
            "        }\n"
            "        if (bearingFromGun == 0) {\n"
            "            scan();\n"
            "        }\n"
            "    }\n\n"

            "    private void bodyBlowFire(ScannedRobotEvent e) {\n"
            "        turnRight(e.getBearing());\n"
            "        if (getHeading() - getGunHeading() != 0) {\n"
            "            turnGunRight(getHeading() - getGunHeading());\n"
            "        } else if (e.getDistance() < 50) {\n"
            "            fire(3);\n"
            "            scan();\n"
            "            return;\n"
            "        }\n"
            "        ahead(e.getDistance() + 5);\n"
            "        scan();\n"
            "    }\n\n"

            "    private void smartFire(ScannedRobotEvent e) {\n"
            "        if (e.getDistance() < 50 && getEnergy() > 50) {\n"
            "            fire(3);\n"
            "        } else {\n"
            "            fire(1);\n"
            "        }\n"
            "        scan();\n"
            "    }\n\n"

            "    private void getAwayFromWall(HitWallEvent e) {\n"
            "        double bearing = e.getBearing();\n"
            "        if (bearing > -90 && bearing < 90) {\n"
            "            setBack(200);\n"
            "        } else {\n"
            "            setAhead(200);\n"
            "        }\n"
            "    }\n\n"

            "    private void turnFlewBullet(HitByBulletEvent e) {\n"
            "        turnRight(e.getHeading() - getHeading() + 180);\n"
            "        turnGunRight(getHeading() - getGunHeading());\n"
            "        scan();\n"
            "    }\n\n"

            "    private void hesitateNextBullet(HitByBulletEvent e) {\n"
            "        turnRight(normalRelativeAngleDegrees(90 - (getHeading() - e.getHeading())));\n"
            "        ahead(100);\n"
            "    }\n\n"

            "    private void turnBodyToEnemy(HitRobotEvent e) {\n"
            "        double turnAmt = normalRelativeAngleDegrees(e.getBearing() + getHeading() - getGunHeading());\n"
            "        turnRight(turnAmt);\n"
            "        if (getHeading() - getGunHeading() != 0) {\n"
            "            turnGunRight(getHeading() - getGunHeading());\n"
            "        }\n"
            "        scan();\n"
            "    }\n\n"

            "    private void turnGunToEnemy(HitRobotEvent e) {\n"
            "        double turnAmt = normalRelativeAngleDegrees(e.getBearing() + getHeading() - getGunHeading());\n"
            "        turnGunRight(turnAmt);\n"
            "        scan();\n"
            "    }\n\n"

            "    private void victoryDance() {\n"
            "        for (int i = 0; i < 50; i++) {\n"
            "            turnRight(30);\n"
            "            turnLeft(30);\n"
            "        }\n"
            "    }\n\n"

            "    private void victorySpin() {\n"
            "        turnRight(36000);\n"
            "    }\n\n";
    }

    std::wstring PathLeaf(const std::filesystem::path& path)
    {
        return path.filename().wstring();
    }

    bool WriteAndCompileJava(const std::filesystem::path& javaPath)
    {
        const std::string root = "C:\\robocode";
        const std::string javac = "C:\\Java\\jdk-17.0.7+7\\bin\\javac.exe";
        const std::string logPath = root + "\\javac_log.txt";
        const std::string batPath = root + "\\compile_temp.bat";

        std::error_code ec;
        std::filesystem::create_directories(javaPath.parent_path(), ec);
        if (ec)
        {
            MessageBoxW(nullptr, L"保存先フォルダを作成できませんでした。", L"保存エラー", MB_OK | MB_ICONERROR);
            return false;
        }

        {
            std::ofstream bat(batPath, std::ios::binary | std::ios::trunc);
            if (!bat.is_open())
            {
                MessageBoxW(nullptr, L"コンパイル用バッチファイルを作成できませんでした。", L"エラー", MB_OK | MB_ICONERROR);
                return false;
            }
            bat << "@echo off\r\nchcp 65001 > nul\r\n";
            bat << "\"" << javac << "\" -encoding UTF-8 -source 8 -target 8 -d \""
                << root << "\\robots\" -cp \"" << root << "\\libs\\robocode.jar\" \""
                << javaPath.string() << "\" > \"" << logPath << "\" 2>&1\r\n";
            bat << "exit /b %ERRORLEVEL%\r\n";
        }

        const int compileResult = std::system(("\"" + batPath + "\"").c_str());
        std::ifstream logFile(logPath, std::ios::binary);
        std::string output((std::istreambuf_iterator<char>(logFile)), std::istreambuf_iterator<char>());
        if (output.empty())
            output = compileResult == 0 ? "Java compile succeeded." : "Java compile failed.";

        if (compileResult != 0)
        {
            MessageBoxA(nullptr, output.c_str(), "javac Result", MB_OK | MB_ICONERROR);
            return false;
        }

        MessageBoxW(nullptr, L"Javaコードを生成し、コンパイルしました。Robocodeを起動します。",
            L"Java出力成功", MB_OK | MB_ICONINFORMATION);
        const std::string launchCommand = "start \"Robocode\" /D \"" + root + "\" cmd /c robocode.bat";
        std::system(launchCommand.c_str());
        return true;
    }
}

std::wstring SelectSavePath()
{
    wchar_t fileName[MAX_PATH] = L"BlockRobot.java";
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = nullptr;
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"Java Files (*.java)\0*.java\0All Files (*.*)\0*.*\0";
    ofn.lpstrDefExt = L"java";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    if (GetSaveFileNameW(&ofn)) return fileName;
    return L"";
}

bool ExportJavaCustom(
    const std::vector<Block>& blocks,
    const std::wstring& targetFolder,
    const std::wstring& robotName)
{
    return ExportJavaCustom(blocks, targetFolder, robotName,
        std::vector<UserDefinedFunction>{});
}

bool ExportJavaCustom(
    const std::vector<Block>& blocks,
    const std::wstring& targetFolder,
    const std::wstring& robotName,
    const std::vector<UserDefinedFunction>& functions)
{
    return ExportJavaCustom(
        blocks,
        std::vector<int>{},
        std::vector<int>{},
        targetFolder,
        robotName,
        functions);
}

bool ExportJavaCustom(
    const std::vector<Block>& blocks,
    const std::vector<int>& parents,
    const std::vector<int>& repeatBodies,
    const std::wstring& targetFolder,
    const std::wstring& robotName,
    const std::vector<UserDefinedFunction>& functions)
{
    if (targetFolder.empty() || robotName.empty())
    {
        MessageBoxW(nullptr, L"ロボット名と保存先フォルダを入力してください。", L"入力エラー", MB_OK | MB_ICONWARNING);
        return false;
    }
    if (!IsJavaIdentifier(robotName))
    {
        MessageBoxW(nullptr,
            L"ロボット名はJavaのクラス名として使える英数字・_・$で入力してください。\n"
            L"先頭に数字は使えません。例：MyRobot_01",
            L"ロボット名を確認してください", MB_OK | MB_ICONWARNING);
        return false;
    }

    std::filesystem::path dirPath(targetFolder);
    std::wstring packageNameWide = PathLeaf(dirPath);
    if (packageNameWide == L"robots" || packageNameWide.empty())
    {
        packageNameWide.clear();
        dirPath = L"C:\\robocode\\robots";
    }
    else if (!IsJavaIdentifier(packageNameWide))
    {
        MessageBoxW(nullptr,
            L"保存先フォルダ名がJavaのパッケージ名として使えません。\n"
            L"英数字・_・$のみのフォルダ名にしてください。",
            L"保存先フォルダを確認してください", MB_OK | MB_ICONWARNING);
        return false;
    }

    const std::filesystem::path javaPath = dirPath / (robotName + L".java");
    const std::string className = WideToUtf8(robotName);
    const std::string packageName = WideToUtf8(packageNameWide);

    std::vector<ExpandedBlock> expanded;
    std::vector<std::wstring> callStack;
    std::wstring expansionError;
    if (!ExpandGraphAndCalls(
        blocks, parents, repeatBodies, functions,
        callStack, expanded, expansionError, 0))
    {
        MessageBoxW(nullptr, expansionError.c_str(), L"自作関数の変換エラー", MB_OK | MB_ICONERROR);
        return false;
    }

    // ★案2：ブロックの種類に応じて、それぞれのメソッド用リストへ自動振り分けを行う
    std::vector<ExpandedBlock> runBlocks;
    std::vector<ExpandedBlock> scannedRobotBlocks;
    std::vector<ExpandedBlock> hitWallBlocks;
    std::vector<ExpandedBlock> hitByBulletBlocks;
    std::vector<ExpandedBlock> hitRobotBlocks;
    std::vector<ExpandedBlock> winBlocks;

    for (const ExpandedBlock& item : expanded)
    {
        if (!item.syntheticRepeatEnd)
        {
            if (item.block.type == BlockType::KeepAwayRadarFire ||
                item.block.type == BlockType::BodyBlowFire ||
                item.block.type == BlockType::SmartFire)
            {
                scannedRobotBlocks.push_back(item);
            }
            else if (item.block.type == BlockType::GetAwayFromWall)
            {
                hitWallBlocks.push_back(item);
            }
            else if (item.block.type == BlockType::HesitateNextBullet)
            {
                hitByBulletBlocks.push_back(item);
            }
            else if (item.block.type == BlockType::TurnBodyToEnemy ||
                item.block.type == BlockType::TurnGunToEnemy)
            {
                hitRobotBlocks.push_back(item);
            }
            else if (item.block.type == BlockType::VictoryDance ||
                item.block.type == BlockType::VictorySpin)
            {
                winBlocks.push_back(item);
            }
            else
            {
                runBlocks.push_back(item);
            }
        }
        else
        {
            // repeat などの閉じタグは runBlocks にフォールバック
            runBlocks.push_back(item);
        }
    }

    const JavaIdentifiers identifiers = BuildJavaIdentifiers(expanded);
    std::string code;
    if (!packageName.empty())
        code += "package " + packageName + ";\n\n";

    code +=
        "import robocode.*;\n"
        "import java.awt.Color;\n"
        "import static robocode.util.Utils.normalRelativeAngleDegrees;\n\n"
        "public class " + className + " extends AdvancedRobot\n"
        "{\n";

    EmitVariableFields(code, identifiers);

    // 1. run() メソッドの出力
    code +=
        "    @Override\n"
        "    public void run()\n"
        "    {\n"
        "        while (true)\n"
        "        {\n";
    EmitBlockSequence(runBlocks, identifiers, code, 3);
    EmitLine(code, 3, "execute();");
    code +=
        "        }\n"
        "    }\n\n";

    // 2. onScannedRobot メソッドの出力
    code +=
        "    @Override\n"
        "    public void onScannedRobot(ScannedRobotEvent e)\n"
        "    {\n";
    EmitBlockSequence(scannedRobotBlocks, identifiers, code, 2);
    code += "    }\n\n";

    // 3. onHitWall メソッドの出力
    code +=
        "    @Override\n"
        "    public void onHitWall(HitWallEvent e)\n"
        "    {\n";
    EmitBlockSequence(hitWallBlocks, identifiers, code, 2);
    code += "    }\n\n";

    // 4. onBulletHit メソッドの出力
    code +=
        "    @Override\n"
        "    public void onBulletHit(BulletHitEvent e) {}\n\n";

    // 5. onHitByBullet メソッドの出力
    code +=
        "    @Override\n"
        "    public void onHitByBullet(HitByBulletEvent e)\n"
        "    {\n";
    EmitBlockSequence(hitByBulletBlocks, identifiers, code, 2);
    code += "    }\n\n";

    // 6. onHitRobot メソッドの出力
    code +=
        "    @Override\n"
        "    public void onHitRobot(HitRobotEvent e)\n"
        "    {\n";
    EmitBlockSequence(hitRobotBlocks, identifiers, code, 2);
    code += "    }\n\n";

    // 7. onWin メソッドの出力
    code +=
        "    @Override\n"
        "    public void onWin(WinEvent e)\n"
        "    {\n";
    EmitBlockSequence(winBlocks, identifiers, code, 2);
    code += "    }\n\n";

    // サンプル互換のヘルパー関数群を出力[cite: 2]
    EmitHelperMethods(code);

    code += "}\n";

    std::error_code ec;
    std::filesystem::create_directories(dirPath, ec);
    if (ec)
    {
        MessageBoxW(nullptr, L"保存先フォルダを作成できませんでした。", L"保存エラー", MB_OK | MB_ICONERROR);
        return false;
    }

    {
        std::ofstream file(javaPath, std::ios::binary | std::ios::trunc);
        if (!file.is_open())
        {
            MessageBoxW(nullptr, L"Javaファイルを保存できませんでした。", L"保存エラー", MB_OK | MB_ICONERROR);
            return false;
        }
        file.write(code.data(), static_cast<std::streamsize>(code.size()));
        if (!file)
        {
            MessageBoxW(nullptr, L"Javaファイルの書き込み中にエラーが発生しました。", L"保存エラー", MB_OK | MB_ICONERROR);
            return false;
        }
    }

    return WriteAndCompileJava(javaPath);
}