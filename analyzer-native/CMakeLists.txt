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

# The main program wires its dependencies up with boost::di.
find_path(BEXT_DI_INCLUDE_DIRS "boost/di.hpp")

# The recorder's public headers (device.h, keycode.h, datetime.h) are used by
# the analyzer even in the browser build, which has no recording support.
set(RECORDER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../recorder)

# The recorder is embedded as a static library instead of being spawned as an
# external process. The browser build has no access to input devices, so it
# leaves the recorder out. When this project is built on its own the recorder
# is added here, otherwise the top level project already did that.
if (CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR AND NOT EMSCRIPTEN)
    if (NOT EXISTS ${RECORDER_DIR}/CMakeLists.txt)
        message(FATAL_ERROR "Cannot find the recorder project at ${RECORDER_DIR}")
    endif()
    add_subdirectory(${RECORDER_DIR} ${CMAKE_CURRENT_BINARY_DIR}/recorder EXCLUDE_FROM_ALL)
endif()

find_package(CMakeRC CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(implot CONFIG REQUIRED)
find_package(boost_json CONFIG REQUIRED)
find_package(pffft CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(simdutf CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
if (NOT EMSCRIPTEN)
    find_package(boost_program_options CONFIG REQUIRED)
    find_package(OpenGL REQUIRED)
    find_package(tinyfiledialogs CONFIG REQUIRED)
endif()

set(ANALYZER_SOURCES
    src/main.cpp

    src/app/app.cpp
    src/app/file_dialog.cpp
    src/app/screenshot.cpp
    src/app/theme.cpp

    src/core/analyzer.cpp
    src/core/decimate.cpp
    src/core/fft.cpp
    src/core/format.cpp
    src/core/input_timeline.cpp
    src/core/keycode_util.cpp
    src/core/postprocess.cpp

    src/parser/parser.cpp
    src/parser/parser_kbi_legacy.cpp
    src/parser/usb_descriptors.cpp

    src/ui/charts_common.cpp
    src/ui/info_window.cpp
    src/ui/line_chart.cpp
    src/ui/timeline_chart.cpp
    src/ui/log_window.cpp
)

if (NOT EMSCRIPTEN)
    list(APPEND ANALYZER_SOURCES
        src/app/save_recording.cpp
        src/recorder/recorder_source.cpp
    )
endif()

add_executable(analyzer-native ${ANALYZER_SOURCES})

# Embed the font into the executable with cmrc (the `cmakerc` vcpkg port).
cmrc_add_resource_library(
    analyzer-resources
    ALIAS analyzer::resources
    NAMESPACE Kbi
    WHENCE ${CMAKE_CURRENT_SOURCE_DIR}/src
    src/font/CascadiaMono.ttf
)
if (EMSCRIPTEN)
    # cmrc compiles its own object files: they need pthreads for the same
    # reason as the analyzer (wasm-ld --shared-memory).
    target_compile_options(analyzer-resources PRIVATE "SHELL:-pthread")
endif()

target_include_directories(analyzer-native PRIVATE src ${BEXT_DI_INCLUDE_DIRS})

if (EMSCRIPTEN)
    # The analyzer only uses the recorder's platform independent headers and
    # helpers for parsing and decoding the recordings.
    target_include_directories(analyzer-native PRIVATE ${RECORDER_DIR}/include)
    target_sources(analyzer-native PRIVATE
        ${RECORDER_DIR}/src/core/datetime.cpp
        ${RECORDER_DIR}/src/core/keycode/keycode_to_string.cpp
    )
    # The vcpkg glfw port would add -sUSE_GLFW=3, selecting the legacy GLFW
    # implementation. The contrib port is selected with the flags below
    # instead, so it must not be linked here.
    target_link_libraries(analyzer-native PRIVATE
        analyzer::resources
        imgui::imgui
        implot::implot
        Boost::json
        pffft::pffft
        simdutf::simdutf
        spdlog::spdlog
    )
else()
    target_link_libraries(analyzer-native PRIVATE
        recorder-lib
        analyzer::resources
        imgui::imgui
        implot::implot
        Boost::json
        Boost::program_options
        pffft::pffft
        glfw
        OpenGL::GL
        simdutf::simdutf
        spdlog::spdlog
        tinyfiledialogs::tinyfiledialogs
    )
endif()

if (UNIX AND NOT APPLE AND NOT EMSCRIPTEN)
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

if (EMSCRIPTEN)
    # Emscripten appends the .html file next to the .js/.wasm pair, which makes
    # the build directly servable.
    set_target_properties(analyzer-native PROPERTIES SUFFIX ".html")
    # The FFT runs on a worker thread. Emscripten implements std::thread with
    # pthreads, so the target has to be compiled and linked with -pthread.
    # The contrib GLFW port supplies the GLFW 3.5 headers and selects that
    # implementation at link time. Its headers need C++17, while vcpkg builds
    # ImGui as C++11, so the port is enabled for this target only.
    target_compile_options(analyzer-native PRIVATE
        "SHELL:-pthread"
        "SHELL:--use-port=contrib.glfw3"
    )
    target_link_options(analyzer-native PRIVATE
        # Browsers cannot run a blocking main loop, the frames are driven by
        # requestAnimationFrame (see main.cpp).
        "SHELL:-sALLOW_MEMORY_GROWTH=1"
        "SHELL:-sINITIAL_MEMORY=67108864"
        "SHELL:-sSTACK_SIZE=1048576"
        # GLFW 3.5 from the contrib port, replacing the legacy -sUSE_GLFW=3.
        "SHELL:--use-port=contrib.glfw3"
        # The FFT worker is created on the main thread before the main loop
        # starts yielding to the browser, so its Worker has to exist already.
        "SHELL:-pthread"
        "SHELL:-sPTHREAD_POOL_SIZE=1"
        # The file dialog suspends the wasm stack until the browser closed the
        # picker. Asyncify is what lets the synchronous C++ code wait for the
        # asynchronously picked file.
        "SHELL:-sASYNCIFY"
        "SHELL:-sASYNCIFY_STACK_SIZE=131072"
        # boost::json reports malformed recordings with exceptions.
        "SHELL:-sDISABLE_EXCEPTION_CATCHING=0"
        # WebGL 2 for the GL context. ImGui's backend picks its GLES2 path
        # automatically on Emscripten (GLSL 100, see main.cpp).
        "SHELL:-sMAX_WEBGL_VERSION=2"
        "SHELL:-sENVIRONMENT=web"
        "SHELL:--js-library=${CMAKE_CURRENT_SOURCE_DIR}/web/file_dialog.js"
        "SHELL:--shell-file=${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html"
    )
    # Package the build output for static hosting (Cloudflare Pages): the
    # entry point under the conventional index.html name, plus the _headers
    # file with the cross-origin isolation headers that the pthread build
    # needs (see web/_headers).
    add_custom_command(TARGET analyzer-native POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/web/_headers
            ${CMAKE_CURRENT_BINARY_DIR}/_headers
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_BINARY_DIR}/analyzer-native.html
            ${CMAKE_CURRENT_BINARY_DIR}/index.html
    )
endif()

# Tests ---------------------------------------------------------------------

if (NOT EMSCRIPTEN)
    enable_testing()
    find_package(ut CONFIG REQUIRED)

    # Every test is its own executable, so a failing or crashing test cannot
    # take down the others. The sources under test are shared between them.
    set(TEST_SUPPORT_SOURCES
        src/core/analyzer.cpp
        src/core/decimate.cpp
        src/core/fft.cpp
        src/core/format.cpp
        src/core/input_timeline.cpp
        src/core/keycode_util.cpp
        src/core/postprocess.cpp

        src/parser/parser.cpp
        src/parser/parser_kbi_legacy.cpp
        src/parser/usb_descriptors.cpp
        src/ui/charts_common.cpp
    )

    function(add_component_test name)
        add_executable(${name}-tests tests/${name}/${name}.cpp ${TEST_SUPPORT_SOURCES})
        target_include_directories(${name}-tests PRIVATE src)
        target_compile_definitions(${name}-tests PRIVATE
            KBI_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/data")
        target_link_libraries(${name}-tests PRIVATE
            Boost::ut
            recorder-lib
            Boost::json
            pffft::pffft
            simdutf::simdutf
            imgui::imgui
            implot::implot
        )
        if (UNIX AND NOT APPLE)
            find_package(Threads REQUIRED)
            target_link_libraries(${name}-tests PRIVATE Threads::Threads)
        endif()
        add_test(NAME ${name}-tests COMMAND ${name}-tests)
    endfunction()

    foreach(name IN ITEMS
        analyzer
        chart_interaction
        datetime
        decimate
        fft
        log_store
        parser
        point_snapper
        postprocess
        result
        rolling_mean
        timeline
        usb_descriptors
    )
        add_component_test(${name})
    endforeach()

    # The reference-analysis dump is a tool, not a test: it produces data for
    # tests/reference_analyzer.py to compare against.
    add_executable(analyzer-native-dump tests/dump/dump.cpp ${TEST_SUPPORT_SOURCES})
    target_include_directories(analyzer-native-dump PRIVATE src)
    target_link_libraries(analyzer-native-dump PRIVATE
        recorder-lib
        Boost::json
        pffft::pffft
        simdutf::simdutf
        imgui::imgui
        implot::implot
    )
    if (UNIX AND NOT APPLE)
        find_package(Threads REQUIRED)
        target_link_libraries(analyzer-native-dump PRIVATE Threads::Threads)
    endif()
endif()
