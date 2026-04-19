cmake_minimum_required(VERSION 3.20)
project(picv VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Set subsystem as Windows so we don't get a command console popping up
add_executable(picv WIN32 main.cpp)

# Link Windows libraries
target_link_libraries(picv PRIVATE 
    d2d1 
    dwrite 
    windowscodecs 
    shcore
    shlwapi 
)

# Optional: Disable CRT secure warnings if we use standard C functions
target_compile_definitions(picv PRIVATE _CRT_SECURE_NO_WARNINGS)
