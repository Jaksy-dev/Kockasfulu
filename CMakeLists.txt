cmake_minimum_required(VERSION 3.16)
project(Kockasfulu VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Use Ninja if available
if(NOT CMAKE_GENERATOR STREQUAL "Ninja")
    message(STATUS "It's recommended to use Ninja for faster builds.")
endif()

# Add source files
add_executable(${PROJECT_NAME}
    src/main.cpp
)

# Include directories
target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${PROJECT_SOURCE_DIR}/external
        ${PROJECT_SOURCE_DIR}/include
)

# Warning flags and optimization
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(${PROJECT_NAME} PRIVATE
        -Wall -Wextra -Werror           # Warnings
        # -Ofast -march=native -flto 
        #-DNDEBUG           # Optimization
        -g
    )
endif()
