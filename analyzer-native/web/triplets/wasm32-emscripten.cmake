# Overlay of vcpkg's community wasm32-emscripten triplet.
#
# The analyzer computes the FFT on an Emscripten pthread. A build that links
# `-pthread` uses shared memory, and wasm-ld rejects any object file that was
# not compiled with the atomics/bulk-memory features. The dependencies have to
# therefore be built with the same thread support as the analyzer itself, which
# is what this overlay adds to the upstream triplet.
#
# IMGUI_IMPL_OPENGL_ES3 switches ImGui's OpenGL3 backend to GLES3 for WebGL2.
# vcpkg builds ImGui as a static library, so the define has to be present when
# the backend is compiled; the vcpkg port only does that for iOS and Android.
include("${VCPKG_ROOT_DIR}/triplets/community/wasm32-emscripten.cmake")

set(VCPKG_C_FLAGS "-pthread")
set(VCPKG_CXX_FLAGS "-pthread -DIMGUI_IMPL_OPENGL_ES3")
set(VCPKG_LINKER_FLAGS "-pthread")
