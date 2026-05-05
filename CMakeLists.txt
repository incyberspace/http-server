cmake_minimum_required(VERSION 3.30)

project(HTTP)

if (NOT WIN32)
    message(FATAL_ERROR "This program is only for windows")
endif ()

add_library(HTTP_LIB STATIC
        src/server.cpp
        src/server.hpp
        src/utils.cpp
        src/utils.hpp
        src/config.cpp
        src/config.hpp
        src/socket.cpp
        src/socket.hpp
        src/async.cpp
        src/async.hpp
)

set_target_properties(HTTP_LIB PROPERTIES CXX_EXTENSIONS OFF)
target_compile_features(HTTP_LIB PUBLIC cxx_std_23)

target_compile_options(HTTP_LIB PUBLIC -Wall -Wextra -pedantic-errors -fno-omit-frame-pointer -Wno-aix-compat)
target_link_options(HTTP_LIB PUBLIC)
target_compile_definitions(HTTP_LIB PRIVATE UNICODE)

add_subdirectory(spdlog)
target_link_libraries(HTTP_LIB PUBLIC spdlog::spdlog)
target_link_libraries(HTTP_LIB PRIVATE "ws2_32")

add_executable(HTTP src/main.cpp)
target_link_libraries(HTTP PRIVATE HTTP_LIB)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
include(GoogleTest)
add_subdirectory(googletest)

add_executable(TEST src/test.cpp)
target_compile_features(TEST PUBLIC cxx_std_23)
target_compile_options(TEST PRIVATE -Wall -Wextra -pedantic-errors)
target_link_libraries(TEST PRIVATE GTest::gtest_main HTTP_LIB)
gtest_discover_tests(TEST WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
