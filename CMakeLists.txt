cmake_minimum_required(VERSION 3.20)
project(CanNodeSimulator VERSION 0.1.0 LANGUAGES CXX)

include(FetchContent)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(CANNODESIM_BUILD_TESTS "Build tests" ON)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/yaml-cpp/CMakeLists.txt")
    set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
    set(YAML_CPP_INSTALL OFF CACHE BOOL "" FORCE)
    add_subdirectory(external/yaml-cpp)

    if(TARGET yaml-cpp::yaml-cpp)
        set(CANNODESIM_YAML_TARGET yaml-cpp::yaml-cpp)
    elseif(TARGET yaml-cpp)
        set(CANNODESIM_YAML_TARGET yaml-cpp)
    else()
        message(FATAL_ERROR "yaml-cpp target not found after adding submodule")
    endif()
else()
    message(FATAL_ERROR "yaml-cpp submodule missing: external/yaml-cpp")
endif()

find_package(spdlog QUIET)
if(NOT spdlog_FOUND)
    FetchContent_Declare(
        spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG v1.15.3)
    FetchContent_MakeAvailable(spdlog)
endif()

add_library(can_node_sim_core
    src/Config.cpp
    src/Model.cpp
    src/EdsParser.cpp
    src/PdoBuilder.cpp
    src/RobotKeywords.cpp
    src/Simulator.cpp
    src/SocketCanTransport.cpp)

target_include_directories(can_node_sim_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(can_node_sim_core PUBLIC ${CANNODESIM_YAML_TARGET} spdlog::spdlog)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(can_node_sim_core PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
endif()

add_executable(CanNodeSimulator src/main.cpp)
target_link_libraries(CanNodeSimulator PRIVATE can_node_sim_core)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/RobotRemoteServerCpp/CMakeLists.txt")
    set(ROBOT_REMOTE_ENABLE_HTTP_SERVER ON CACHE BOOL "" FORCE)
    add_subdirectory(external/RobotRemoteServerCpp)
    if(TARGET robot_remote)
        target_link_libraries(CanNodeSimulator PRIVATE robot_remote)
        target_compile_definitions(CanNodeSimulator PRIVATE CANNODESIM_HAS_ROBOT_REMOTE=1)
    endif()
endif()

if(CANNODESIM_BUILD_TESTS)
    include(CTest)
    add_subdirectory(tests)
endif()
