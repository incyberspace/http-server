cmake_minimum_required(VERSION 3.30)

project(HTTP)
enable_testing()

if (NOT WIN32)
    message(FATAL_ERROR "This program is only for windows")
endif ()

SET(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(ENABLE_ASAN OFF CACHE BOOL "" FORCE)

if (ENABLE_ASAN)
    add_compile_options(-MD -fsanitize=address,undefined)
    link_libraries(clang_rt.asan_dynamic-x86_64 clang_rt.asan_dynamic_runtime_thunk-x86_64)
    add_link_options(/wholearchive:clang_rt.asan_dynamic_runtime_thunk-x86_64.lib)
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
target_compile_definitions(HTTP_LIB PUBLIC
        SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_DEBUG
)

if (MINGW)
    target_compile_options(HTTP_LIB PUBLIC -Wall -Wextra -pedantic-errors -fno-omit-frame-pointer -Wno-aix-compat)
endif ()

target_link_options(HTTP_LIB PUBLIC)
target_compile_definitions(HTTP_LIB PRIVATE UNICODE)

add_subdirectory(spdlog)
target_link_libraries(HTTP_LIB PUBLIC spdlog::spdlog)

set(HTTPLIB_USE_NON_BLOCKING_GETADDRINFO OFF CACHE BOOL "" FORCE)
add_subdirectory(cpp-httplib)
target_link_libraries(HTTP_LIB PUBLIC httplib::httplib)

target_link_libraries(HTTP_LIB PRIVATE "ws2_32")

add_executable(HTTP src/main.cpp)
target_link_libraries(HTTP PRIVATE HTTP_LIB)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
include(GoogleTest)
add_subdirectory(googletest)

add_executable(TEST src/test.cpp)
target_compile_features(TEST PUBLIC cxx_std_23)

if (MINGW)
    target_compile_options(TEST PRIVATE -Wall -Wextra -pedantic-errors)
endif ()

target_link_libraries(TEST PRIVATE GTest::gtest_main HTTP_LIB)
gtest_discover_tests(TEST WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} DISCOVERY_MODE PRE_TEST)

add_custom_target(COPY_COMPILE_COMMANDS ALL COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/compile_commands.json ${CMAKE_SOURCE_DIR})
