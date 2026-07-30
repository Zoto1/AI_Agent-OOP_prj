cmake_minimum_required(VERSION 3.10)

# 1. Khai báo tên dự án
project(OopAgent VERSION 1.0 LANGUAGES CXX)

# 2. Ép chuẩn C++
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 3. Tìm các thư viện bên thứ 3 (Dependencies)
find_package(CURL REQUIRED)
find_package(nlohmann_json REQUIRED)

# 4. Chỉ định thư mục chứa file header (.h)
include_directories(${CMAKE_SOURCE_DIR}/src)

# 5. Gom nhóm mã nguồn từ các module thành một Static Library
# Gom toàn bộ file .cpp trong các thư mục con
file(GLOB_RECURSE SRC_FILES 
    "src/agent/*.cpp"
    "src/client/*.cpp"
    "src/tools/*.cpp"
    "src/harness/*.cpp"
)

# Tạo thư viện tĩnh từ mã nguồn
add_library(OopAgentCore STATIC ${SRC_FILES})

# Liên kết thư viện bên thứ ba vào thư viện tĩnh của dự án
target_link_libraries(OopAgentCore PUBLIC 
    CURL::libcurl 
    nlohmann_json::nlohmann_json
)

# 6. Tạo file thực thi chính (Executable)
add_executable(agent_run main.cpp)

# Liên kết thư viện tĩnh nội bộ vào file thực thi
target_link_libraries(agent_run PRIVATE OopAgentCore)

add_executable(benchmark_run src/benchmark/run_eval.cpp)
target_link_libraries(benchmark_run PRIVATE OopAgentCore)