cmake_minimum_required(VERSION 3.13)
project(sigslot)

# Include the Conan toolchain
include(${CMAKE_CURRENT_SOURCE_DIR}/conan_toolchain.cmake)

# GoogleTest requires at least C++14, coroutines need C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(GTest)
find_package(sentry)
enable_testing()
add_executable(sigslot-test
        test/sigslot.cc
        test/main.cpp
)
target_link_libraries(sigslot-test PUBLIC gtest::gtest sentry-native::sentry-native)
target_include_directories(sigslot-test SYSTEM PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(sigslot-test PRIVATE DWD_GTEST_SENTRY=1)
include(GoogleTest)
gtest_discover_tests(sigslot-test)

if (UNIX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fcoroutines")
endif ()
if (WIN32)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /await")
endif()

install(DIRECTORY sigslot TYPE INCLUDE)