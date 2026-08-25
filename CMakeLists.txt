cmake_minimum_required(VERSION 3.20)
project(Nai-Long-Adventure CXX)

include(FetchContent)
FetchContent_Declare(sfml GIT_REPOSITORY https://github.com/SFML/SFML.git GIT_TAG 3.0.2)
FetchContent_MakeAvailable(sfml)

add_executable(Nai-Long-Adventure main.cpp "Application.h" "Application.cpp")
target_compile_features(Nai-Long-Adventure PRIVATE cxx_std_20)
target_link_libraries(Nai-Long-Adventure PRIVATE SFML::Graphics SFML::Audio)