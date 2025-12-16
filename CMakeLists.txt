cmake_minimum_required(VERSION 3.16)
project(xBase3 VERSION 0.1.0 LANGUAGES C)

# C11 standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Compiler warnings
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    add_compile_options(-Wall -Wextra -pedantic -Werror=implicit-function-declaration)
endif()

# Debug/Release settings
set(CMAKE_C_FLAGS_DEBUG "-g -O0 -DDEBUG")
set(CMAKE_C_FLAGS_RELEASE "-O2 -DNDEBUG")

# Default to Debug if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# Source files
set(XBASE3_SOURCES
    src/main.c
    src/util.c
    src/dbf.c
    src/lexer.c
    src/ast.c
    src/parser.c
    src/expr.c
    src/functions.c
    src/variables.c
    src/commands.c
)

# Main executable
add_executable(xbase3 ${XBASE3_SOURCES})

# Include directories
target_include_directories(xbase3 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Link math library on Unix
if(UNIX)
    target_link_libraries(xbase3 PRIVATE m)
endif()

# Check for readline (optional, for better REPL experience)
find_library(READLINE_LIBRARY readline)
find_path(READLINE_INCLUDE_DIR readline/readline.h)

if(READLINE_LIBRARY AND READLINE_INCLUDE_DIR)
    message(STATUS "Found readline: ${READLINE_LIBRARY}")
    target_compile_definitions(xbase3 PRIVATE HAVE_READLINE)
    target_include_directories(xbase3 PRIVATE ${READLINE_INCLUDE_DIR})
    target_link_libraries(xbase3 PRIVATE ${READLINE_LIBRARY})
else()
    message(STATUS "readline not found, using basic input")
endif()

# Enable testing
enable_testing()
add_subdirectory(tests)

# Install target
install(TARGETS xbase3 RUNTIME DESTINATION bin)

# Print configuration summary
message(STATUS "")
message(STATUS "xBase3 Configuration:")
message(STATUS "  Version:      ${PROJECT_VERSION}")
message(STATUS "  Build type:   ${CMAKE_BUILD_TYPE}")
message(STATUS "  C Compiler:   ${CMAKE_C_COMPILER}")
message(STATUS "  C Flags:      ${CMAKE_C_FLAGS}")
message(STATUS "")
