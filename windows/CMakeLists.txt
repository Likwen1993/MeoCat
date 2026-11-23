cmake_minimum_required(VERSION 3.20)
project(MeoCat VERSION 1.0 LANGUAGES CXX)
enable_language(RC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(MeoCat WIN32
    src/main.cpp
)

target_compile_definitions(MeoCat PRIVATE _WIN32_WINNT=0x0601)
target_include_directories(MeoCat PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
if(MSVC)
    target_compile_options(MeoCat PRIVATE /O2 /utf-8 /EHsc /W3)
    set_property(TARGET MeoCat PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    target_sources(MeoCat PRIVATE app.manifest version.rc)
else()
    target_sources(MeoCat PRIVATE app.rc)
    target_compile_options(MeoCat PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-O2>)
    target_link_options(MeoCat PRIVATE -municode -static)
endif()

target_link_libraries(MeoCat PRIVATE user32 gdi32 gdiplus shell32 advapi32 crypt32 dwmapi)
