cmake_minimum_required(VERSION 3.28)

project(flashcards)

set(CMAKE_CXX_STANDARD 20)
add_executable(flashcards src/main.cpp src/card.cpp src/json_io.cpp)
target_include_directories(flashcards PRIVATE ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/rapidjson/include)
target_compile_options(flashcards PRIVATE -Wall -Wextra -Wconversion
  -Werror=pedantic -Werror)
