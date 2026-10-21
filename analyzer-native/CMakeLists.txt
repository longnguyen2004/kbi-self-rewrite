cmake_minimum_required(VERSION 3.24)

project(analyzer-native LANGUAGES C CXX)

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

# The recorder is embedded as a static library instead of being spawned as an
# external process. When this project is built on its own the recorder is
# added here, otherwise the top level project already did that.
if (CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(RECORDER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../recorder)
    if (NOT EXISTS ${RECORDER_DIR}/CMakeLists.txt)
        message(FATAL_ERROR "Cannot find the recorder project at ${RECORDER_DIR}")
    endif()
    add_subdirectory(${RECORDER_DIR} ${CMAKE_CURRENT_BINARY_DIR}/recorder EXCLUDE_FROM_ALL)
endif()

find_package(imgui CONFIG REQUIRED)
find_package(implot CONFIG REQUIRED)
find_package(boost_json CONFIG REQUIRED)
find_package(pffft CONFIG REQUIRED)
find_package(simdutf CONFIG REQUIRED)
find_package(tinyfiledialogs CONFIG REQUIRED)
find_package(OpenGL REQUIRED)

add_executable(analyzer-native
    src/main.cpp

    src/app/app.cpp
    src/app/file_dialog.cpp
    src/app/screenshot.cpp
    src/app/theme.cpp

    src/core/analyzer.cpp
    src/core/decimate.cpp
    src/core/fft.cpp
    src/core/input_timeline.cpp
    src/core/keycode_util.cpp
    src/core/postprocess.cpp

    src/parser/parser.cpp
    src/parser/parser_kbi_legacy.cpp
    src/parser/usb_descriptors.cpp

    src/recorder/recorder_source.cpp

    src/ui/charts.cpp
    src/ui/info_window.cpp
)

target_include_directories(analyzer-native PRIVATE src)

target_link_libraries(analyzer-native PRIVATE
    recorder-lib
    imgui::imgui
    implot::implot
    Boost::json
    pffft::pffft
    simdutf::simdutf
    tinyfiledialogs::tinyfiledialogs
    OpenGL::GL
)

if (UNIX AND NOT APPLE)
    find_package(Threads REQUIRED)
    target_link_libraries(analyzer-native PRIVATE Threads::Threads)
endif()

if (WIN32)
    target_link_libraries(analyzer-native PRIVATE
        dwmapi
        shell32
        imm32
    )
endif()

set_target_properties(analyzer-native PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    WIN32_EXECUTABLE OFF
)

# Tests ---------------------------------------------------------------------

enable_testing()

add_executable(analyzer-native-tests
    tests/tests.cpp

    src/core/analyzer.cpp
    src/core/decimate.cpp
    src/core/fft.cpp
    src/core/input_timeline.cpp
    src/core/keycode_util.cpp
    src/core/postprocess.cpp

    src/parser/parser.cpp
    src/parser/parser_kbi_legacy.cpp
    src/parser/usb_descriptors.cpp
    src/ui/charts.cpp
)
target_include_directories(analyzer-native-tests PRIVATE src)
target_link_libraries(analyzer-native-tests PRIVATE
    recorder-lib
    Boost::json
    pffft::pffft
    simdutf::simdutf
    imgui::imgui
    implot::implot
)
if (UNIX AND NOT APPLE)
    find_package(Threads REQUIRED)
    target_link_libraries(analyzer-native-tests PRIVATE Threads::Threads)
endif()

add_test(NAME analyzer-native-tests
         COMMAND analyzer-native-tests
                 ${CMAKE_CURRENT_SOURCE_DIR}/tests/data/test_recording.json
                 ${CMAKE_CURRENT_SOURCE_DIR}/tests/data/test_recording_legacy.kbi)
