# Custom Emscripten toolchain for Nix
# Fixes the em-config path issue in nixpkgs emscripten

# Find emcc and derive paths
find_program(EMCC_EXECUTABLE emcc REQUIRED)
get_filename_component(EMSCRIPTEN_BIN_DIR "${EMCC_EXECUTABLE}" DIRECTORY)
get_filename_component(EMSCRIPTEN_PREFIX "${EMSCRIPTEN_BIN_DIR}" DIRECTORY)
set(EMSCRIPTEN_ROOT_PATH "${EMSCRIPTEN_PREFIX}/share/emscripten" CACHE PATH "Emscripten root")

# Get cache dir using em-config from bin/
execute_process(
  COMMAND "${EMSCRIPTEN_BIN_DIR}/em-config" "CACHE"
  RESULT_VARIABLE _emcache_result
  OUTPUT_VARIABLE _emcache_output
  OUTPUT_STRIP_TRAILING_WHITESPACE
  ERROR_QUIET
)

if(_emcache_result EQUAL 0)
  file(TO_CMAKE_PATH "${_emcache_output}" EMSCRIPTEN_SYSROOT_CACHE)
  set(EMSCRIPTEN_SYSROOT "${EMSCRIPTEN_SYSROOT_CACHE}/sysroot" CACHE PATH "Emscripten sysroot")
else()
  # Fallback: try to find sysroot in share/emscripten/cache
  set(EMSCRIPTEN_SYSROOT "${EMSCRIPTEN_ROOT_PATH}/cache/sysroot" CACHE PATH "Emscripten sysroot")
endif()

# Set up cross-compilation
set(CMAKE_SYSTEM_NAME Emscripten)
set(CMAKE_SYSTEM_VERSION 1)
set(CMAKE_CROSSCOMPILING TRUE)

# Compilers
set(CMAKE_C_COMPILER "${EMSCRIPTEN_BIN_DIR}/emcc")
set(CMAKE_CXX_COMPILER "${EMSCRIPTEN_BIN_DIR}/em++")
set(CMAKE_AR "${EMSCRIPTEN_BIN_DIR}/emar" CACHE FILEPATH "Emscripten ar")
set(CMAKE_RANLIB "${EMSCRIPTEN_BIN_DIR}/emranlib" CACHE FILEPATH "Emscripten ranlib")

# Sysroot
set(CMAKE_SYSROOT "${EMSCRIPTEN_SYSROOT}")
set(CMAKE_FIND_ROOT_PATH "${EMSCRIPTEN_SYSROOT}")

# Search paths
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Emscripten-specific settings
set(EMSCRIPTEN ON)
set(CMAKE_EXECUTABLE_SUFFIX ".js")

message(STATUS "Emscripten Nix toolchain:")
message(STATUS "  EMCC: ${CMAKE_C_COMPILER}")
message(STATUS "  Sysroot: ${EMSCRIPTEN_SYSROOT}")
