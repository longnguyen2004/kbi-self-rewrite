# Overlay of vcpkg's community wasm32-emscripten triplet.
#
# The analyzer computes the FFT on an Emscripten pthread. A build that links
# `-pthread` uses shared memory, and wasm-ld rejects any object file that was
# not compiled with the atomics/bulk-memory features (for example
# imgui_draw.cpp.o). The dependencies therefore have to be built with the same
# thread support as the analyzer itself, which is what this overlay adds to the
# upstream triplet.
#
# The contrib GLFW port is not enabled here: its headers require C++17, while
# vcpkg builds ImGui as C++11. The port flags are applied to the analyzer
# target only (see analyzer-native/CMakeLists.txt).
include("${VCPKG_ROOT_DIR}/triplets/community/wasm32-emscripten.cmake")

set(VCPKG_C_FLAGS "-pthread")
set(VCPKG_CXX_FLAGS "-pthread")
set(VCPKG_LINKER_FLAGS "-pthread")
