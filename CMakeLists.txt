cmake_minimum_required(VERSION 3.24)

project(keyboard-inspector LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# IntelliSense reads compile_commands.json to pick up the real compile flags
# (notably -std=c++23). It is only generated for Makefile/Ninja generators.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if (MSVC)
    add_compile_options(/Zc:__cplusplus /utf-8)
endif()

# boost::unordered's concurrent_flat_map ABI depends on this define, and the
# recorder's public headers expose it, so it has to match recorder-lib exactly.
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    add_compile_definitions(-D_GLIBCXX_USE_TBB_PAR_BACKEND=0)
endif()

# The recorder and the analyzer look for the same packages. Sharing the
# imported targets keeps them from being defined twice.
set(CMAKE_FIND_PACKAGE_TARGETS_GLOBAL ON)

enable_testing()

add_subdirectory(recorder)
add_subdirectory(analyzer-native)

# Style targets ---------------------------------------------------------------
#
# The sources follow the WebKit code style, see .clang-format and .clang-tidy
# in the repository root. The scripts in Tools/ do the actual work so that
# they can also be used without CMake.
if (CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    find_program(KBI_CLANG_FORMAT NAMES clang-format)
    find_program(KBI_CLANG_TIDY NAMES clang-tidy)

    if (KBI_CLANG_FORMAT)
        add_custom_target(format
            COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/Tools/format.sh
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            COMMENT "Formatting the sources with clang-format"
            VERBATIM
        )

        add_custom_target(format-check
            COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/Tools/format.sh --check
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            COMMENT "Checking the formatting with clang-format"
            VERBATIM
        )
    else()
        message(STATUS "clang-format not found, the format targets are not available")
    endif()

    if (KBI_CLANG_TIDY)
        add_custom_target(tidy
            COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/Tools/tidy.sh
                    --build-dir ${CMAKE_BINARY_DIR}
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            COMMENT "Running clang-tidy over the sources"
            VERBATIM
        )
    else()
        message(STATUS "clang-tidy not found, the tidy target is not available")
    endif()
endif()
