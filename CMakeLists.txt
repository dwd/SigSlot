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
link_libraries(GTest::gtest_main)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
add_executable(sigslot-test
        test/sigslot.cc
        test/coroutine.cc
        sigslot/sigslot.h
        sigslot/tasklet.h
        sigslot/resume.h
)
add_executable(sigslot-test-resume
        sigslot/sigslot.h
        sigslot/tasklet.h
        test/resume.cc
        sigslot/resume.h
)
add_executable(sigslot-test-cothread
        sigslot/sigslot.h
        sigslot/tasklet.h
        test/cothread.cc
        sigslot/resume.h
        sigslot/cothread.h
)
include(GoogleTest)
gtest_discover_tests(sigslot-test)
gtest_discover_tests(sigslot-test-resume)
gtest_discover_tests(sigslot-test-cothread)

if (UNIX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fcoroutines")
endif ()
if (WIN32)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /await")
endif()
