cmake_minimum_required(VERSION 3.10)
project(mshm C)

option(SHM_BUILD_TESTS "Build test programs" ON)

add_compile_definitions(SHM_PLATFORM_LINUX=1)
add_compile_options(-Wall -Wextra -pedantic)

include_directories(${CMAKE_SOURCE_DIR}/include)

add_subdirectory(src)

if(SHM_BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()
