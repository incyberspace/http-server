cmake_minimum_required(VERSION 4.0)

project(HTTP)

add_library(HTTP_LIB STATIC
        src/server.cpp
        src/server.hpp
        src/utils.cpp
        src/utils.hpp
        src/config.cpp
        src/config.hpp
)
target_compile_features(HTTP_LIB PUBLIC cxx_std_23)
target_compile_options(HTTP_LIB PUBLIC -Wall -Wextra -pedantic-errors)

add_subdirectory(spdlog)
target_link_libraries(HTTP_LIB PUBLIC spdlog::spdlog)

add_subdirectory(boost)
target_link_libraries(HTTP_LIB PUBLIC Boost::headers)
target_link_libraries(HTTP_LIB PRIVATE Boost::asio)

if (WIN32)
    target_link_libraries(HTTP_LIB PRIVATE "ws2_32")
endif ()

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
