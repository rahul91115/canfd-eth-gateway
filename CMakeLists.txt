cmake_minimum_required(VERSION 3.14)
project(canfd-gateway VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(gateway src/main.cpp)

# Ensure no exceptions/RTTI (good for embedded)
target_compile_options(gateway PRIVATE
    -fno-exceptions
    -fno-rtti
    -Wall
    -Wextra
    -Werror
    -O2
)

# Link against real-time library (for clock_gettime)
target_link_libraries(gateway PRIVATE -lrt)
