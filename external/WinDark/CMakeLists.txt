cmake_minimum_required(VERSION 3.25)

add_library(WinDark STATIC
    src/WinDark.cpp
    include/WinDark/WinDark.h
)

target_include_directories(WinDark PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_compile_definitions(WinDark PRIVATE
    UNICODE
    _UNICODE
    STRICT
    NOMINMAX
    WIN32_LEAN_AND_MEAN
)

target_compile_options(WinDark PRIVATE
    /W4
    /permissive-
    /utf-8
    /Zc:__cplusplus
    /EHsc
)

target_link_libraries(WinDark PUBLIC
    uxtheme
    dwmapi
    gdi32
    user32
    advapi32
)

set_target_properties(WinDark PROPERTIES FOLDER "external")
