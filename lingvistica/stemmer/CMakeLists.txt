cmake_minimum_required(VERSION 3.10)
project(stemmer)

set(CMAKE_CXX_STANDARD 17)

add_library(stemmer stemmer.cpp)

find_package(GTest REQUIRED)

add_executable(stemmer_tests test_stemmer.cpp)
target_link_libraries(stemmer_tests stemmer GTest::gtest GTest::gtest_main)

enable_testing()
add_test(stemmer_unit_tests stemmer_tests)