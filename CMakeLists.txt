cmake_minimum_required(VERSION 3.13)
project(sigslot)

set (CMAKE_CXX_STANDARD 17)

include_directories(.)
add_executable(example example.cc sigslot/sigslot.h)

add_executable(co_example co_example.cc sigslot/sigslot.h)
if (UNIX)
    target_compile_options(co_example PUBLIC -fcoroutines-ts -stdlib=libc++)
    target_link_options(co_example PUBLIC -fcoroutines-ts -stdlib=libc++)
    target_link_libraries(co_example pthread)
endif ()
if (WIN32)
    target_compile_options(co_example PUBLIC /await)
endif()
target_compile_definitions(co_example PUBLIC -DSIGSLOT_COROUTINES)
