cmake_minimum_required(VERSION 3.14)
project(BooleanIndexer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(${PROJECT_SOURCE_DIR})
include_directories(${PROJECT_SOURCE_DIR}/..)
include_directories(${PROJECT_SOURCE_DIR}/../lingvistica/tokenizer)
include_directories(${PROJECT_SOURCE_DIR}/../lingvistica/stemmer)

add_executable(indexer
    main.cpp
    indexer.cpp
    search_engine.cpp
    ../lingvistica/stemmer/stemmer.cpp
    ../lingvistica/tokenizer/tokenizer.cpp
)