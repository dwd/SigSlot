cmake_minimum_required(VERSION 3.13)
project(sigslot)

# GoogleTest requires at least C++14, coroutines need C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)
FetchContent_MakeAvailable(googletest)

enable_testing()
add_executable(sigslot-test
        test/sigslot.cc
        test/coroutine.cc
        sigslot/sigslot.h
        sigslot/tasklet.h
        sigslot/resume.h
)
target_link_libraries(sigslot-test
        GTest::gtest_main
)
target_include_directories(sigslot-test
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)
add_executable(sigslot-test-resume
        sigslot/sigslot.h
        sigslot/tasklet.h
        test/resume.cc
        sigslot/resume.h
)
target_link_libraries(sigslot-test-resume
        GTest::gtest_main
)
target_include_directories(sigslot-test-resume
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)
include(GoogleTest)
gtest_discover_tests(sigslot-test)
gtest_discover_tests(sigslot-test-resume)

if (UNIX)
    target_compile_options(sigslot-test PUBLIC -fcoroutines)
    target_link_options(sigslot-test PUBLIC -fcoroutines)
    target_compile_options(sigslot-test-resume PUBLIC -fcoroutines)
    target_link_options(sigslot-test-resume PUBLIC -fcoroutines)
endif ()
if (WIN32)
    target_compile_options(sigslot-test-resume PUBLIC /await)
    target_compile_options(sigslot-test PUBLIC /await)
endif()
