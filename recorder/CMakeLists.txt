cmake_minimum_required(VERSION 3.15)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

project(recorder LANGUAGES C CXX)

if (MSVC)
    add_compile_options(/Zc:__cplusplus)
endif()

add_library(recorder-lib STATIC
    src/core/recorder/recorder.cpp
    src/core/keycode/keycode_to_string.cpp
)

find_package(boost_container CONFIG REQUIRED)
find_package(boost_endian CONFIG REQUIRED)
find_package(boost_unordered CONFIG REQUIRED)
find_package(boost_iterator CONFIG REQUIRED)
find_package(boost_signals2 CONFIG REQUIRED)
find_package(date CONFIG REQUIRED)
find_package(utf8cpp CONFIG REQUIRED)
target_link_libraries(recorder-lib
    PUBLIC Boost::signals2
    PRIVATE Boost::container Boost::unordered Boost::iterator Boost::endian date::date
)

if (WIN32)
    find_package(gameinput CONFIG REQUIRED)
    find_package(wil CONFIG REQUIRED)
    
    target_link_libraries(recorder-lib PRIVATE Microsoft::GameInput)
    target_link_libraries(recorder-lib PRIVATE WIL::WIL)
    target_link_libraries(recorder-lib PRIVATE Hid SetupAPI cfgmgr32)

    target_sources(recorder-lib PRIVATE
        src/core/recorder/win/vk_to_keycode.cpp
        src/core/recorder/win/device_name.cpp
        src/core/recorder/win/recorder_win_gameinput.cpp
    )
elseif(LINUX)
    find_package(PkgConfig)
    pkg_check_modules(LIBEVDEV REQUIRED libevdev)
    pkg_check_modules(LIBSYSTEMD REQUIRED libsystemd)
    target_link_libraries(recorder-lib PRIVATE ${LIBEVDEV_LINK_LIBRARIES} ${LIBSYSTEMD_LINK_LIBRARIES})
    target_include_directories(recorder-lib PRIVATE ${LIBEVDEV_INCLUDE_DIRS} ${LIBSYSTEMD_INCLUDE_DIRS})

    find_package(Threads REQUIRED)
    target_link_libraries(recorder-lib PRIVATE Threads::Threads)

    target_sources(recorder-lib PRIVATE
        src/core/recorder/linux/evdev_to_keycode.cpp
        src/core/recorder/linux/device_name.cpp
        src/core/recorder/linux/recorder_linux_libevdev.cpp
    )
else()
    message(FATAL_ERROR "A recorder hasn't been implemented for this platform")
endif()
target_include_directories(recorder-lib PUBLIC include)

add_executable(recorder-app
    src/exporter/exporter.h
    src/exporter/exporter_mat_kbi.cpp
    src/portable_getch.c
    src/main.cpp
)
target_link_libraries(recorder-app PRIVATE recorder-lib)
