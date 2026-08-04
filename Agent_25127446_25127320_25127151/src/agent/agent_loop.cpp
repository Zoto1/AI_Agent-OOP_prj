#include "agent_loop.h"
#include "loop_detector.h"
#include "../tools/tool_registry.h"
#include "skill_loader.h"
#include <iostream>
#include <map>
#include <variant>
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

std::string AgentLoop::run(const std::string &task)
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
                std::cerr << "[LoopDetector] " << loop_result.message << "\n";
                if (this->step_hook)
                {
                    this->step_hook(cur_step);
                }
                return "Agent stopped because the loop detector flagged a repeating pattern.";
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
            return answer.text;
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
        }
    }

    return "[RUN_OUT_OF_STEPS] Agent stopped: reached max_steps (" +
           std::to_string(max_steps) + ") without producing a final answer.";
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

    std::string response = llm->chat(history);
    return response;
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
    size_t start = response.find('{');
    size_t end = response.rfind('}');

    if (start == std::string::npos || end == std::string::npos || end < start)
    {
        return std::nullopt;
    }

    std::string jsonStr = response.substr(start, end - start + 1);

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
        "1. To call a tool, respond with ONLY a JSON object, no other text:\n"
        "   {\"tool\": \"<tool_name>\", \"args\": {\"<key>\": \"<value>\"}}\n"
        "2. If you already have enough info to give the final answer, "
        "respond in plain text (not JSON).\n"
        "3. Only use tool names from the list above.\n\n";

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