cmake_minimum_required(VERSION 3.24)

project(keyboard-inspector LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

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
