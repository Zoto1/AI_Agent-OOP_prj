cmake_minimum_required(VERSION 3.25)

# ==================================================
# Project
# ==================================================
project(OopAgent VERSION 1.0 LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ==================================================
# C++ Standard
# ==================================================
# Nâng lên C++26 để hỗ trợ:
#   C++20: Concepts (LLMBackend), std::jthread
#   C++23: std::expected<T,E>, std::print / std::println
#   C++26: deleted functions with diagnostic messages
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ==================================================
# Dependencies
# ==================================================
# Trong WSL, FindCURL co the vo tinh nap CURLConfig.cmake cua MSYS2 tu
# /mnt/c. Buoc build Linux phai dung FindCURL module va thu vien Linux.
if(UNIX AND NOT APPLE)
    set(CURL_NO_CURL_CMAKE TRUE)
endif()

find_package(CURL QUIET)
if(NOT CURL_FOUND)
    message(FATAL_ERROR
        "Khong tim thay libcurl development cho Linux. Cai bang:\n"
        "  sudo apt install libcurl4-openssl-dev zlib1g-dev")
endif()

find_package(nlohmann_json QUIET)
if(NOT nlohmann_json_FOUND)
    message(FATAL_ERROR
        "Khong tim thay nlohmann/json. Cai bang:\n"
        "  sudo apt install nlohmann-json3-dev")
endif()

find_package(SQLite3 QUIET)
if(NOT SQLite3_FOUND)
    message(FATAL_ERROR
        "Khong tim thay libsqlite3 development. Cai bang:\n"
        "  sudo apt install libsqlite3-dev")
endif()

# ==================================================
# Include
# ==================================================
include_directories(${CMAKE_SOURCE_DIR}/src)

# ==================================================
# Source files
# ==================================================
file(GLOB_RECURSE SRC_FILES
    "src/agent/*.cpp"
    "src/client/*.cpp"
    "src/tools/*.cpp"
    "src/harness/*.cpp"
)

# ==================================================
# Core Library
# ==================================================
add_library(OopAgentCore STATIC
    ${SRC_FILES}
)

target_link_libraries(
    OopAgentCore
    PUBLIC
    CURL::libcurl
    nlohmann_json::nlohmann_json
    SQLite::SQLite3
)

# ==================================================
# Main executable
# ==================================================
add_executable(
    agent_run
    main.cpp
)

target_link_libraries(
    agent_run
    PRIVATE
    OopAgentCore
)

# ==================================================
# Benchmark executable
# ==================================================
add_executable(
    benchmark_run
    src/benchmark/run_eval.cpp
)

target_link_libraries(
    benchmark_run
    PRIVATE
    OopAgentCore
)

# ==================================================
# Multi-Agent Demo executable
# ==================================================
add_executable(
    demo_multi_agent
    demo_multi_agent.cpp
)

target_link_libraries(
    demo_multi_agent
    PRIVATE
    OopAgentCore
)

# ==================================================
# Testing
# ==================================================
enable_testing()

# --------------------------------------------------
# Keyword Evaluator
# --------------------------------------------------
add_executable(
    test_keyword_evaluator
    src/tests/test_keyword_evaluator.cpp
)

target_link_libraries(
    test_keyword_evaluator
    PRIVATE
    OopAgentCore
)

add_test(
    NAME KeywordEvaluatorTest
    COMMAND test_keyword_evaluator
)

# --------------------------------------------------
# Functional Evaluator
# --------------------------------------------------
add_executable(
    test_functional_evaluator
    src/tests/test_functional_evaluator.cpp
)

target_link_libraries(
    test_functional_evaluator
    PRIVATE
    OopAgentCore
)

add_test(
    NAME FunctionalEvaluatorTest
    COMMAND test_functional_evaluator
)

# --------------------------------------------------
# Trajectory
# --------------------------------------------------
add_executable(
    test_trajectory
    src/tests/test_trajectory.cpp
)

target_link_libraries(
    test_trajectory
    PRIVATE
    OopAgentCore
)

add_test(
    NAME TrajectoryTest
    COMMAND test_trajectory
)

# --------------------------------------------------
# Termination Status
# --------------------------------------------------
add_executable(
    test_termination_status
    src/tests/test_termination_status.cpp
)

target_link_libraries(
    test_termination_status
    PRIVATE
    OopAgentCore
)

add_test(
    NAME TerminationStatusTest
    COMMAND test_termination_status
)

# --------------------------------------------------
# Agent tool-call integration
# --------------------------------------------------
add_executable(
    test_agent_tool_call
    src/tests/test_agent_tool_call.cpp
)

target_link_libraries(
    test_agent_tool_call
    PRIVATE
    OopAgentCore
)

add_test(
    NAME AgentToolCallTest
    COMMAND test_agent_tool_call
)

# --------------------------------------------------
# Gemini multipart/function-call parsing
# --------------------------------------------------
add_executable(
    test_gemini_response
    src/tests/test_gemini_response.cpp
)

target_link_libraries(
    test_gemini_response
    PRIVATE
    OopAgentCore
)

add_test(
    NAME GeminiResponseTest
    COMMAND test_gemini_response
)

add_executable(
    test_multi_agent
    src/tests/test_multi_agent.cpp
)

target_link_libraries(
    test_multi_agent
    PRIVATE
    OopAgentCore
)

add_test(
    NAME MultiAgentTest
    COMMAND test_multi_agent
)

add_executable(
    test_tools
    src/tests/test_tools.cpp
)

target_link_libraries(
    test_tools
    PRIVATE
    OopAgentCore
)

add_test(
    NAME ToolsTest
    COMMAND test_tools
)

# --------------------------------------------------
# Exec tool: stdout, stderr, exit code and timeout
# --------------------------------------------------
add_executable(
    test_exec_tool
    src/tests/test_exec_tool.cpp
)

target_link_libraries(
    test_exec_tool
    PRIVATE
    OopAgentCore
)

add_test(
    NAME ExecToolTest
    COMMAND test_exec_tool
)

# --------------------------------------------------
# Harness end-to-end with a fake LLM (no external API)
# --------------------------------------------------
add_executable(
    test_harness_integration
    src/tests/test_harness_integration.cpp
)

target_link_libraries(
    test_harness_integration
    PRIVATE
    OopAgentCore
)

add_test(
    NAME HarnessIntegrationTest
    COMMAND test_harness_integration
)

# --------------------------------------------------
# Memory embedding + persistence
# --------------------------------------------------
add_executable(
    test_embedding_memory
    src/tests/test_embedding_memory.cpp
)

target_link_libraries(
    test_embedding_memory
    PRIVATE
    OopAgentCore
)

add_test(
    NAME EmbeddingMemoryTest
    COMMAND test_embedding_memory
)
