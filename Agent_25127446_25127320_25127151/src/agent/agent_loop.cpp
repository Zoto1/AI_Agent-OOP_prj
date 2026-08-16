#include "agent_loop.h"
#include "loop_detector.h"
#include "../tools/tool_registry.h"
#include "skill_loader.h"
#include <chrono>
#include <iostream>
#include <map>
#include <regex>
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

    std::optional<json> parseJsonObjectFromResponse(const std::string &response)
    {
        const std::string whitespace = " \t\n\r";
        const std::size_t first = response.find_first_not_of(whitespace);
        if (first == std::string::npos)
        {
            return std::nullopt;
        }

        std::string candidate = response.substr(first);
        if (candidate.rfind("```", 0) == 0)
        {
            const std::size_t first_newline = candidate.find('\n');
            const std::size_t closing_fence = candidate.rfind("```");
            if (first_newline != std::string::npos &&
                closing_fence != std::string::npos &&
                closing_fence > first_newline)
            {
                candidate = candidate.substr(
                    first_newline + 1,
                    closing_fence - first_newline - 1);
            }
        }

        bool in_string = false;
        bool escaped = false;
        int depth = 0;
        std::size_t object_start = std::string::npos;

        for (std::size_t index = 0; index < candidate.size(); ++index)
        {
            const char character = candidate[index];
            if (escaped)
            {
                escaped = false;
                continue;
            }
            if (character == '\\' && in_string)
            {
                escaped = true;
                continue;
            }
            if (character == '"')
            {
                in_string = !in_string;
                continue;
            }
            if (in_string)
            {
                continue;
            }
            if (character == '{')
            {
                if (depth == 0)
                {
                    object_start = index;
                }
                ++depth;
            }
            else if (character == '}' && depth > 0)
            {
                --depth;
                if (depth == 0 && object_start != std::string::npos)
                {
                    try
                    {
                        json parsed = json::parse(candidate.substr(
                            object_start, index - object_start + 1));
                        if (parsed.is_object())
                        {
                            return parsed;
                        }
                    }
                    catch (const json::exception &)
                    {
                        return std::nullopt;
                    }
                }
            }
        }

        return std::nullopt;
    }

    std::string escapeControlCharactersInJsonStrings(const std::string &text)
    {
        static constexpr char hex_digits[] = "0123456789abcdef";
        std::string sanitized;
        sanitized.reserve(text.size());

        bool in_string = false;
        bool escaped = false;

        for (const char character : text)
        {
            if (escaped)
            {
                sanitized.push_back(character);
                escaped = false;
                continue;
            }

            if (character == '\\' && in_string)
            {
                sanitized.push_back(character);
                escaped = true;
                continue;
            }

            if (character == '"')
            {
                sanitized.push_back(character);
                in_string = !in_string;
                continue;
            }

            const auto byte = static_cast<unsigned char>(character);
            if (in_string && byte < 0x20)
            {
                sanitized += "\\u00";
                sanitized.push_back(hex_digits[(byte >> 4) & 0x0f]);
                sanitized.push_back(hex_digits[byte & 0x0f]);
                continue;
            }

            sanitized.push_back(character);
        }

        return sanitized;
    }

    bool looksLikeStructuredActionAttempt(const std::string &response)
    {
        const std::size_t first = response.find_first_not_of(" \t\n\r");
        if (first == std::string::npos)
        {
            return false;
        }

        return response[first] == '{' ||
               response.compare(first, 3, "```") == 0 ||
               response.find("\"tool\"") != std::string::npos ||
               response.find("tool_call") != std::string::npos;
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

    bool has_executed_tool = false;
    long long run_total_tokens = 0;

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
        const auto think_started = std::chrono::steady_clock::now();
        LLMResponse model_response = this->think();
        cur_step.tokens_used = model_response.usage.total_tokens;
        run_total_tokens += model_response.usage.total_tokens;
        cur_step.latency_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                  std::chrono::steady_clock::now() - think_started)
                                  .count();

        std::optional<std::variant<ToolCall, FinalAnswer>> parsed_action;
        const bool native_tool_call = model_response.tool_call.has_value();
        std::string thought;

        if (native_tool_call)
        {
            const LLMToolCall &native_call = *model_response.tool_call;
            ToolCall call{native_call.name, native_call.args};
            parsed_action = call;
            thought = json({
                {"type", "tool_call"},
                {"tool", call.tool},
                {"args", json::parse(call.args)}
            }).dump();
        }
        else
        {
            thought = model_response.text;
            parsed_action = this->act(thought);

            // A native function-calling provider normally returns plain text
            // after a tool result. Treat that text as final only after at least
            // one real tool execution; before then, prose is a protocol error.
            if (!parsed_action.has_value() &&
                has_executed_tool &&
                !thought.empty() &&
                !looksLikeStructuredActionAttempt(thought))
            {
                parsed_action = FinalAnswer{thought};
            }
        }

        cur_step.thought = thought;

        if (this->verbose)
        {
            std::cout << thought << std::endl;
        }

        Message assistant_message;
        assistant_message.role = "assistant";
        assistant_message.content = thought;
        if (native_tool_call)
        {
            const LLMToolCall &native_call = *model_response.tool_call;
            assistant_message.kind = MessageKind::FunctionCall;
            assistant_message.tool_name = native_call.name;
            assistant_message.tool_args = native_call.args;
            assistant_message.tool_call_id = native_call.id;
            assistant_message.thought_signature = native_call.thought_signature;
        }
        this->history.push_back(assistant_message);

        if (!parsed_action.has_value())
        {
            Message correction;
            correction.role = "user";
            correction.content =
                "FORMAT_ERROR: Your previous response was only a plan or used an invalid format. "
                "Act now. Call one of the declared functions. If native function calling is unavailable, "
                "return ONLY {\"type\":\"tool_call\",\"tool\":\"tool_name\",\"args\":{...}}. "
                "If the task is completely finished, return ONLY "
                "{\"type\":\"final_answer\",\"answer\":\"...\"}.";
            this->history.push_back(correction);

            if (this->verbose)
            {
                std::cout << "[FORMAT ERROR] Model output was not an action; retrying.\n";
            }
            continue;
        }

        // ACT
        cur_step.action = *parsed_action;
        this->step_history.push_back(cur_step);

        if (this->loop_detector)
        {
            LoopResult loop_result = this->loop_detector->detect(this->step_history);
            if (loop_result.type != LoopType::NONE)
            {
                if (loop_result.sev == LoopSeverity::CRITICAL)
                {
                    if (this->step_hook)
                    {
                        this->step_hook(cur_step);
                    }
                    return {"", AgentTerminationStatus::LoopDetected,
                            run_total_tokens};
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
            return {answer.text, AgentTerminationStatus::Completed,
                    run_total_tokens};
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
                if (!this->tool_registry->hasTool(tool_call.tool))
                {
                    result = "Error: unknown tool '" + tool_call.tool +
                             "'. Use an exact name from Available tools.";
                }
                else
                {
                    const auto args = parseToolArgs(tool_call.args);
                    result = this->tool_registry->executeTool(tool_call.tool, args);
                }
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

            if (native_tool_call)
            {
                const LLMToolCall &native_call = *model_response.tool_call;
                Message tool_message;
                tool_message.role = "user";
                tool_message.content = result;
                tool_message.kind = MessageKind::FunctionResponse;
                tool_message.tool_name = native_call.name;
                tool_message.tool_call_id = native_call.id;
                this->history.push_back(tool_message);
            }
            else
            {
                this->observe("Tool result for '" + tool_call.tool + "': " + result);
            }
            has_executed_tool = true;
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
                    return {"", AgentTerminationStatus::LoopDetected,
                            run_total_tokens};
                }
            }
        }
    }

    return {"", AgentTerminationStatus::MaxStepsReached, run_total_tokens};
}

void AgentLoop::observe(const std::string &tool_result)
{
    Message tool_message;
    tool_message.role = "tool";
    tool_message.content = tool_result;
    this->history.push_back(tool_message);
}

LLMResponse AgentLoop::think()
{
    // =========================================================================
    // S-3 — C++23: std::expected<T, E>
    // =========================================================================
    // safeChatWithTools() bọc chatWithTools() trong try/catch và trả về
    // std::expected<LLMResponse, std::string>.
    // Nếu có lỗi, result.error() chứa error message — không cần try/catch ở đây.
    const std::string declarations = tool_registry
                                         ? tool_registry->functionDeclarationsJson()
                                         : std::string("[]");
    auto result = llm->safeChatWithTools(history, declarations);

    if (!result.has_value()) {
        // Phân loại lỗi để ném đúng loại exception
        const std::string &err = result.error();
        if (err.find("API_ENVIRONMENT") != std::string::npos ||
            err.find("GEMINI_API_KEY") != std::string::npos)
        {
            throw std::runtime_error(std::string("API_ENVIRONMENT_ERROR: ") + err);
        }
        throw std::runtime_error(std::string("LLM_CLIENT_ERROR: ") + err);
    }

    return result.value();
}

std::optional<std::variant<ToolCall, FinalAnswer>>
AgentLoop::act(const std::string &thought)
{
    if (auto tool_call = this->parseToolCall(thought))
    {
        return std::variant<ToolCall, FinalAnswer>(*tool_call);
    }

    if (auto final_answer = this->parseFinalAnswer(thought))
    {
        return std::variant<ToolCall, FinalAnswer>(*final_answer);
    }

    return std::nullopt;
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
        // Models occasionally place a literal newline/tab inside a quoted JSON
        // value. JSON requires those control characters to be escaped.
        json parsed = json::parse(
            escapeControlCharactersInJsonStrings(jsonStr));

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

std::optional<FinalAnswer>
AgentLoop::parseFinalAnswer(const std::string &response)
{
    const auto parsed = parseJsonObjectFromResponse(response);
    if (!parsed.has_value())
    {
        return std::nullopt;
    }

    const std::string type = parsed->value("type", "");
    if (!type.empty() && type != "final_answer")
    {
        return std::nullopt;
    }

    for (const char *field : {"answer", "final_answer", "text"})
    {
        if (parsed->contains(field) && parsed->at(field).is_string())
        {
            return FinalAnswer{parsed->at(field).get<std::string>()};
        }
    }

    return std::nullopt;
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
        "1. ACT; never return a plan, internal reasoning, or a promise to call a tool.\n"
        "2. Prefer the API's native function call mechanism. Use exact tool names and arguments from the schemas above.\n"
        "3. If native function calling is unavailable, a tool action must be ONLY this JSON shape:\n"
        "   {\"type\":\"tool_call\",\"tool\":\"calculator\",\"args\":{\"expression\":\"128 / 8\"}}\n"
        "4. A completed task must be ONLY this JSON shape:\n"
        "   {\"type\":\"final_answer\",\"answer\":\"The completed result\"}\n"
        "5. Calculations MUST use calculator. Never calculate mentally or return a numeric result before calculator runs.\n"
        "6. File operations, shell commands, memory operations, and searches MUST execute the matching tools.\n"
        "7. After every tool result, either call the next required tool or return the final_answer JSON.\n"
        "8. Do not put JSON in Markdown fences and do not add text outside the JSON fallback.\n\n";

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
