#include "../src/harness/trajectory.h"

#include <cassert>
#include <iostream>

int main()
{
    assert(
        terminationStatusToString(
            TerminationStatus::Unknown
        ) == "unknown"
    );

    assert(
        terminationStatusToString(
            TerminationStatus::Completed
        ) == "completed"
    );

    assert(
        terminationStatusToString(
            TerminationStatus::LoopDetected
        ) == "loop_detected"
    );

    assert(
        terminationStatusToString(
            TerminationStatus::MaxStepsReached
        ) == "max_steps_reached"
    );

    assert(
        terminationStatusToString(
            TerminationStatus::AgentError
        ) == "agent_error"
    );

    assert(
        terminationStatusToString(
            TerminationStatus::EvaluationError
        ) == "evaluation_error"
    );

    assert(
        terminationStatusToString(
            TerminationStatus::EnvironmentError
        ) == "environment_error"
    );

    std::cout
        << "[PASS] TerminationStatus tests\n";

    return 0;
}