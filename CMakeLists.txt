cmake_minimum_required(VERSION 3.22)

project(poker_game
    VERSION 0.1.0
    DESCRIPTION "Multi-process Texas Hold'em poker server"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(POKER_BUILD_TESTS "Build unit tests" ON)
option(POKER_BUILD_SIM "Build the poker simulator" ON)
option(POKER_USE_SYSTEM_DEPS "Use system-installed dependencies" ON)
option(POKER_ENABLE_WARNINGS "Enable compiler warnings" ON)
option(POKER_ENABLE_ASAN "Enable AddressSanitizer (non-MSVC)" OFF)

if(NOT POKER_USE_SYSTEM_DEPS)
    message(FATAL_ERROR
        "POKER_USE_SYSTEM_DEPS=OFF is not supported. "
        "Install dependencies with MSYS2 pacman or the Linux package manager.")
endif()

find_package(nlohmann_json CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(Threads REQUIRED)

add_library(poker_warnings INTERFACE)
if(POKER_ENABLE_WARNINGS)
    if(MSVC)
        target_compile_options(poker_warnings INTERFACE /W4 /permissive-)
    else()
        target_compile_options(poker_warnings INTERFACE
            -Wall
            -Wextra
            -Wpedantic
            -Wshadow)
    endif()
endif()

if(POKER_ENABLE_ASAN AND NOT MSVC)
    target_compile_options(poker_warnings INTERFACE
        -fsanitize=address
        -fno-omit-frame-pointer)
    target_link_options(poker_warnings INTERFACE -fsanitize=address)
endif()

add_library(poker_common STATIC
    src/common/config.cpp
    src/common/ids.cpp
    src/common/json.cpp
    src/common/logging.cpp
    src/common/time.cpp)

target_include_directories(poker_common
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(poker_common
    PUBLIC
        nlohmann_json::nlohmann_json
        spdlog::spdlog
        Threads::Threads
    PRIVATE
        poker_warnings)

if(WIN32)
    target_link_libraries(poker_common PUBLIC ws2_32)
endif()

add_library(poker_core STATIC
    src/poker_core/action.cpp
    src/poker_core/action_validator.cpp
    src/poker_core/card.cpp
    src/poker_core/deck.cpp
    src/poker_core/game_engine.cpp
    src/poker_core/game_state.cpp
    src/poker_core/hand_evaluator.cpp
    src/poker_core/hand_rank.cpp
    src/poker_core/player.cpp
    src/poker_core/rng.cpp
    src/poker_core/table_config.cpp)

target_include_directories(poker_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(poker_core
    PUBLIC
        poker_common
    PRIVATE
        poker_warnings)

if(POKER_BUILD_TESTS)
    enable_testing()
    find_package(GTest REQUIRED)
    add_subdirectory(tests)
endif()

# Process targets (conn_svr, poker_svr, client_svr, poker_sim) and the
# game state machine are added in later milestones. poker_core currently
# contains the deterministic rules primitives.
