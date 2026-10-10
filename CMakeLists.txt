cmake_minimum_required(VERSION 3.20)
project(PTXScope VERSION 0.1.0 LANGUAGES CXX)

include(CTest)
option(PTXSCOPE_ENABLE_SANITIZERS "Enable UndefinedBehaviorSanitizer" OFF)

add_library(ptxscope_core
    src/lexer.cpp
    src/parser.cpp
    src/analysis.cpp
    src/report.cpp)
target_include_directories(ptxscope_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(ptxscope_core PUBLIC cxx_std_20)
if(MSVC)
    target_compile_options(ptxscope_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(ptxscope_core PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
endif()

add_executable(ptxscope src/main.cpp)
target_link_libraries(ptxscope PRIVATE ptxscope_core)
target_compile_features(ptxscope PRIVATE cxx_std_20)
target_compile_definitions(ptxscope PRIVATE PTXSCOPE_VERSION="${PROJECT_VERSION}")

if(PTXSCOPE_ENABLE_SANITIZERS AND NOT MSVC)
    target_compile_options(ptxscope_core PUBLIC -fsanitize=undefined -fno-omit-frame-pointer)
    target_link_options(ptxscope_core PUBLIC -fsanitize=undefined)
endif()

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
