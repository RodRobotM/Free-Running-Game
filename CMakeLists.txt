cmake_minimum_required(VERSION 3.20)
project(clickable_sphere LANGUAGES CXX)

add_executable(clickable_sphere WIN32 main.cpp)
target_compile_features(clickable_sphere PRIVATE cxx_std_17)
target_link_libraries(clickable_sphere PRIVATE user32 gdi32 Comctl32)