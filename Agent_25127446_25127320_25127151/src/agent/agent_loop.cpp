#include "agent_loop.h"
#include <iostream>
#include <variant>


//CONSTURCTOR
AgentLoop::AgentLoop(std::shared_ptr<LLMClient> client,
                     std::shared_ptr<ToolRegistry> registry,
                     std::shared_ptr<SkillLoader> loader,
                     std::shared_ptr<LoopDetector> detector,
                     int max_steps)
    : llm(client), tool_registry(registry), skill_loader(loader),
      loop_detector(detector), max_steps(max_steps) {}


//STEPHOOK
void AgentLoop::setStepHook(std::function<void(const Step &)> hook)
{
    this->step_hook = hook;
}


std::string AgentLoop::run(const std::string &task)
{
    Message mes_system = this->buildSystemMessage(task);
    this->history.push_back(mes_system);

    for (int i = 0; i < this->max_steps; ++i)
    {
        Step cur_step;
        cur_step.step_id = i;

        //THINK
        std::string thought = this->think();
        cur_step.thought = thought;

        //ACT
        cur_step.action = this->act(thought);

        if (std::holds_alternative<FinalAnswer>(cur_step.action))
        {
            FinalAnswer answer = std::get<FinalAnswer>(cur_step.action);
            if (this->step_hook)
            {
                this->step_hook(cur_step);
            }
            return answer.text;
        }

        if (std::holds_alternative<ToolCall>(cur_step.action))
        {
            ToolCall tool_call = std::get<ToolCall>(cur_step.action);
            std::string result = "Kết quả từ Tool: " + tool_call.tool;
            this->observe(result);
            cur_step.tool_result = result;

            if (this->step_hook)
            {
                this->step_hook(cur_step);
            }
        }
    }

    return "Kết quả cuối cùng của AI (Final Answer)";
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
    return "Chuỗi phản hồi suy nghĩ từ AI";
}

std::variant<ToolCall, FinalAnswer> AgentLoop::act(const std::string &thought)
{
    (void)thought;

    FinalAnswer dummy_answer;
    dummy_answer.text = "Giả lập câu trả lời cuối";
    return dummy_answer;
}

Message AgentLoop::buildSystemMessage(const std::string &task)
{
    Message system_message;
    system_message.role = "system";
    system_message.content = "You are an agent. Task: " + task;
    return system_message;
}