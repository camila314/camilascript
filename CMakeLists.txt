cmake_minimum_required(VERSION 4.0.0 FATAL_ERROR)

project(camilascript VERSION 0.1.0 LANGUAGES CXX C)

add_executable(${PROJECT_NAME} main.cpp)
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_20)

target_sources(${PROJECT_NAME}
  PUBLIC
    FILE_SET all_my_modules TYPE CXX_MODULES FILES
    TokenStream.cppm
    Expression.cppm
    State.cppm
    Parser.cppm
)

if (MSVC)
    target_compile_options(${PROJECT_NAME} PUBLIC /wd4996)
else()
    target_compile_options(${PROJECT_NAME} PUBLIC -Wno-deprecated-declarations)
endif()
