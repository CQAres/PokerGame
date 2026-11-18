add_executable(poker_common_tests
    common/common_tests.cpp)

target_link_libraries(poker_common_tests
    PRIVATE
        poker_common
        GTest::gtest_main
        poker_warnings)

add_test(NAME poker_common_tests COMMAND poker_common_tests)

add_executable(poker_core_tests
    core/core_tests.cpp
    core/engine_tests.cpp)

target_link_libraries(poker_core_tests
    PRIVATE
        poker_core
        GTest::gtest_main
        poker_warnings)

add_test(NAME poker_core_tests COMMAND poker_core_tests)

if(WIN32 AND EXISTS "C:/msys64/mingw64/bin")
    set_tests_properties(poker_common_tests poker_core_tests PROPERTIES
        ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:C:/msys64/mingw64/bin")
endif()
