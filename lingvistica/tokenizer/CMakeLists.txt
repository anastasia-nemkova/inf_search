cmake_minimum_required(VERSION 3.10)
project(tokenizer)

set(CMAKE_CXX_STANDARD 17)

add_library(tokenizer tokenizer.cpp)

find_package(GTest REQUIRED)

add_executable(tokenizer_tests test_tokenizer.cpp)
target_link_libraries(tokenizer_tests tokenizer GTest::gtest GTest::gtest_main)

enable_testing()
add_test(tokenizer_unit_tests tokenizer_tests)