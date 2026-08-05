#include "agent_loop.h"
#include "loop_detector.h"
#include "../tools/tool_registry.h"
#include "skill_loader.h"
#include <iostream>
#include <map>
#include <variant>
#include <regex>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace
{
    std::map<std::string, std::string> parseToolArgs(const std::string &raw_args)
    {
        std::map<std::string, std::string> args;

        if (raw_args.empty())
        {
            return args;
        }

        try
        {
            json parsed = json::parse(raw_args);
            if (parsed.is_object())
            {
                for (auto it = parsed.begin(); it != parsed.end(); ++it)
                {
                    if (it.value().is_string())
                    {
                        args[it.key()] = it.value().get<std::string>();
                    }
                    else
                    {
                        args[it.key()] = it.value().dump();
                    }
                }
            }
            else if (parsed.is_string())
            {
                args["input"] = parsed.get<std::string>();
            }
            else
            {
                args["value"] = parsed.dump();
            }
        }
        catch (const json::exception &)
        {
            args["input"] = raw_args;
        }

        return args;
    }
} // namespace

// CONSTURCTOR
AgentLoop::AgentLoop(std::shared_ptr<LLMClient> client,
                     std::shared_ptr<ToolRegistry> registry,
                     std::shared_ptr<SkillLoader> loader,
                     std::shared_ptr<LoopDetector> detector,
                     int max_steps)
    : llm(client), tool_registry(registry), skill_loader(loader),
      loop_detector(detector), max_steps(max_steps) {}

// STEPHOOK
void AgentLoop::setStepHook(std::function<void(const Step &)> hook)
{
    this->step_hook = hook;
}

void AgentLoop::setVerbose(bool enable)
{
    this->verbose = enable;
}

AgentRunResult AgentLoop::run(const std::string &task)
{
    history.clear();
    step_history.clear();

    Message mes_system = buildSystemMessage(task);
    history.push_back(mes_system);

    Message user_message;
    user_message.role = "user";
    user_message.content = task;
    history.push_back(user_message);

    for (int i = 0; i < this->max_steps; ++i)
    {
        Step cur_step;
        cur_step.step_id = i;

        if (this->verbose)
        {
            std::cout << "\n[STEP " << (i + 1) << "]" << std::endl;
            std::cout << "[THINK]" << std::endl;
        }

        // THINK
        std::string thought = this->think();
        cur_step.thought = thought;

        if (this->verbose)
        {
            std::cout << thought << std::endl;
        }

        Message assistant_message;
        assistant_message.role = "assistant";
        assistant_message.content = thought;
        this->history.push_back(assistant_message);

        // ACT
        cur_step.action = this->act(thought);
        this->step_history.push_back(cur_step);

        if (this->loop_detector)
        {
            LoopResult loop_result = this->loop_detector->detect(this->step_history);
            if (loop_result.type != LoopType::NONE)
            {
                if (loop_result.sev == LoopSeverity::CRITICAL)
                {
                    std::cerr << "[LoopDetector] " << loop_result.message << "\n";
                    if (this->step_hook)
                    {
                        this->step_hook(cur_step);
                    }
                    return {"", AgentTerminationStatus::LoopDetected};
                }
                else
                {
                    std::cerr << "[LoopDetector WARNING] " << loop_result.message << "\n";
                }
            }
        }

        if (std::holds_alternative<FinalAnswer>(cur_step.action))
        {
            FinalAnswer answer = std::get<FinalAnswer>(cur_step.action);
            if (this->verbose)
            {
                std::cout << "[FINAL]" << std::endl
                          << answer.text << std::endl;
            }
            if (this->step_hook)
            {
                this->step_hook(cur_step);
            }
            return {answer.text, AgentTerminationStatus::Completed};
        }

        if (std::holds_alternative<ToolCall>(cur_step.action))
        {
            ToolCall tool_call = std::get<ToolCall>(cur_step.action);
            std::string result;

            if (this->verbose)
            {
                std::cout << "[ACTION]" << std::endl;
                std::cout << "Tool: " << tool_call.tool << std::endl;
                std::cout << "Args: " << tool_call.args << std::endl;
            }

            if (this->tool_registry)
            {
                const auto args = parseToolArgs(tool_call.args);
                result = this->tool_registry->executeTool(tool_call.tool, args);
            }
            else
            {
                result = "ToolRegistry is not available for tool: " + tool_call.tool;
            }

            if (this->verbose)
            {
                std::cout << "[TOOL]" << std::endl
                          << result << std::endl;
            }

            this->observe(result);
            cur_step.tool_result = result;

            this->step_history.back().tool_result = result;

            if (this->step_hook)
            {
                this->step_hook(cur_step);
            }
            // Re-run loop detection after tool result is available
            if (this->loop_detector)
            {
                LoopResult post_loop_result = this->loop_detector->detect(this->step_history);
                if (post_loop_result.type != LoopType::NONE && post_loop_result.sev == LoopSeverity::CRITICAL)
                {
                    std::cerr << "[LoopDetector] " << post_loop_result.message << "\n";
                    return {"", AgentTerminationStatus::LoopDetected};
                }
            }
        }
    }

    return {"", AgentTerminationStatus::MaxStepsReached};
}

void AgentLoop::observe(const std::string &tool_result)
{
    Message tool_message;
    tool_message.role = "tool";
    tool_message.content = tool_result;
    this->history.push_back(tool_message);
}

std::string AgentLoop::think()
{
    try
    {
        return llm->chat(history);
    }
    catch (const APIEnvironmentError &e)
    {
        throw std::runtime_error(std::string("API_ENVIRONMENT_ERROR: ") + e.what());
    }
    catch (const LLMClientError &e)
    {
        throw std::runtime_error(std::string("LLM_CLIENT_ERROR: ") + e.what());
    }
}

std::variant<ToolCall, FinalAnswer>
AgentLoop::act(const std::string &thought)
{
    if (auto tool_call = this->parseToolCall(thought))
    {
        return *tool_call;
    }

    FinalAnswer answer;
    answer.text = thought;
    return answer;
}
std::optional<ToolCall> AgentLoop::parseToolCall(const std::string &response)
{
    auto trim = [](const std::string &s) {
        const std::string ws = " \t\n\r";
        size_t a = s.find_first_not_of(ws);
        if (a == std::string::npos)
            return std::string();
        size_t b = s.find_last_not_of(ws);
        return s.substr(a, b - a + 1);
    };

    auto extractJsonObject = [&](const std::string &text) -> std::optional<std::string> {
        bool in_string = false;
        bool escape = false;
        int depth = 0;
        size_t start = std::string::npos;

        for (size_t i = 0; i < text.size(); ++i)
        {
            char c = text[i];
            if (escape)
            {
                escape = false;
                continue;
            }
            if (c == '\\')
            {
                if (in_string)
                {
                    escape = true;
                }
                continue;
            }
            if (c == '"')
            {
                in_string = !in_string;
                continue;
            }
            if (in_string)
            {
                continue;
            }
            if (c == '{')
            {
                if (depth == 0)
                {
                    start = i;
                }
                depth++;
            }
            else if (c == '}' && depth > 0)
            {
                depth--;
                if (depth == 0 && start != std::string::npos)
                {
                    return text.substr(start, i - start + 1);
                }
            }
        }
        return std::nullopt;
    };

    auto repairJson = [&](std::string text) -> std::string {
        // Convert single quotes to double quotes outside strings
        bool in_string = false;
        bool escape = false;
        for (size_t i = 0; i < text.size(); ++i)
        {
            char c = text[i];
            if (escape)
            {
                escape = false;
                continue;
            }
            if (c == '\\')
            {
                escape = true;
                continue;
            }
            if (c == '"')
            {
                in_string = !in_string;
                continue;
            }
            if (!in_string && c == '\'')
            {
                text[i] = '"';
            }
        }

        // Quote bare object keys like {tool: and "tool":
        std::regex keyRegex(R"(([\{\[,]\s*)([A-Za-z_][A-Za-z0-9_]*)\s*:)" );
        text = std::regex_replace(text, keyRegex, "$1\"$2\":");

        // Quote bare string values such as : foo, but not numbers or booleans/null
        std::regex bareStringValue(R"((:\s*)(?!true\b|false\b|null\b|[-0-9\.])([A-Za-z_][A-Za-z0-9_]*)(\s*(?:,|\}|\]|\n|$)))");
        text = std::regex_replace(text, bareStringValue, "$1\"$2\"$3");
        text = std::regex_replace(text, bareStringValue, "$1\"$2\"$3");

        // Remove trailing commas before object/array close
        std::regex trailingComma(R"(,\s*([\}\]]))");
        text = std::regex_replace(text, trailingComma, "$1");

        return text;
    };

    std::string resp = trim(response);
    std::string jsonStr;

    auto normalizeJsonQuotes = [&](std::string text) -> std::string {
        bool in_string = false;
        bool escape = false;
        for (size_t i = 0; i < text.size(); ++i)
        {
            char c = text[i];
            if (escape)
            {
                escape = false;
                continue;
            }
            if (c == '\\')
            {
                escape = true;
                continue;
            }
            if (c == '"')
            {
                in_string = !in_string;
                continue;
            }
            if (!in_string && c == '\'')
            {
                text[i] = '"';
            }
        }
        return text;
    };

    auto parseFenceContent = [&](const std::string &block) -> std::optional<std::string> {
        std::string trimmed_block = trim(block);
        if (trimmed_block.rfind("json", 0) == 0)
        {
            size_t nl = trimmed_block.find('\n');
            if (nl != std::string::npos)
            {
                trimmed_block = trim(trimmed_block.substr(nl + 1));
            }
        }

        auto candidate = extractJsonObject(trimmed_block);
        if (candidate.has_value())
        {
            return candidate;
        }

        if (trimmed_block.find("tool") != std::string::npos &&
            trimmed_block.find("args") != std::string::npos)
        {
            std::string repaired = normalizeJsonQuotes(trimmed_block);
            return extractJsonObject(repaired);
        }

        return std::nullopt;
    };

    size_t fence_start = resp.find("```");
    while (fence_start != std::string::npos)
    {
        size_t fence_end = resp.find("```", fence_start + 3);
        if (fence_end == std::string::npos)
            break;

        auto result = parseFenceContent(resp.substr(fence_start + 3, fence_end - (fence_start + 3)));
        if (result.has_value())
        {
            jsonStr = result.value();
            break;
        }

        fence_start = resp.find("```", fence_end + 3);
    }

    if (jsonStr.empty())
    {
        std::optional<std::string> candidate = extractJsonObject(resp);
        if (!candidate.has_value() && resp.find("tool") != std::string::npos && resp.find("args") != std::string::npos)
        {
            std::string repaired = repairJson(resp);
            candidate = extractJsonObject(repaired);
            if (!candidate.has_value())
            {
                candidate = repaired;
            }
        }
        if (candidate.has_value())
        {
            jsonStr = candidate.value();
        }
    }

    if (jsonStr.empty())
    {
        return std::nullopt;
    }

    try
    {
        json parsed = json::parse(jsonStr);

        if (!parsed.is_object() || !parsed.contains("tool") || !parsed.contains("args"))
        {
            return std::nullopt;
        }

        if (!parsed.at("tool").is_string())
        {
            return std::nullopt;
        }

        ToolCall call;
        call.tool = parsed.at("tool").get<std::string>();

        call.args = parsed.at("args").is_string()
                        ? parsed.at("args").get<std::string>()
                        : parsed.at("args").dump();

        return call;
    }
    catch (const json::exception &)
    {
        return std::nullopt;
    }
}

Message AgentLoop::buildSystemMessage(const std::string &task)
{
    std::string content = "You are an agent. Task: " + task + "\n\n";

    // Liệt kê tool khả dụng cho LLM
    if (tool_registry)
    {
        content += "## Available tools:\n" + tool_registry->describeToolsForPrompt() + "\n\n";
    }

    content +=
        "## Response rules (MUST FOLLOW):\n"
        "1. If you need a tool to answer the task, respond with ONLY a JSON object and nothing else.\n"
        "   Example: {\"tool\": \"calculator\", \"args\": {\"input\": \"128 / 8\"}}\n"
        "2. Do not include any analysis, plan, or explanation outside the JSON object when calling a tool.\n"
        "3. Use only tool names from the list above.\n"
        "4. If the task requires side effects (file write, exec, memory, web search, etc.), you must call the appropriate tool instead of returning a plain-text answer.\n"
        "5. If the task is a calculation, ALWAYS use the calculator tool and do not provide a numeric result directly.\n"
        "6. If the task involves writing, reading, shell execution, memory, or internet search, do not answer directly; call the tool instead.\n"
        "7. If you are unsure whether the task needs a tool, prefer tooling and do not return the answer directly.\n"
        "8. If you already have enough information to give the final answer without calling a tool, return the final answer in plain text only.\n\n";

    // Inject skill phù hợp với task
    if (skill_loader)
    {
        auto skill = skill_loader->select_skill(task);
        if (skill.has_value())
        {
            content += "## Relevant skill guidance:\n" + skill->instruction + "\n\n";
        }
    }

    Message system_message;
    system_message.role = "system";
    system_message.content = content;
    return system_message;
}