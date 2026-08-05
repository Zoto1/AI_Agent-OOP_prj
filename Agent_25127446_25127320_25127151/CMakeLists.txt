cmake_minimum_required(VERSION 3.10)

# ==================================================
# Project
# ==================================================
project(OopAgent VERSION 1.0 LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ==================================================
# C++ Standard
# ==================================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ==================================================
# Dependencies
# ==================================================
find_package(CURL REQUIRED)
find_package(nlohmann_json REQUIRED)

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
