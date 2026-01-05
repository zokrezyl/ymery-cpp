# Filesystem plugin - exposes both tree-like and device-manager interfaces

# For Emscripten: compile plugins as side modules for dynamic linking
if(YMERY_WEB)
    # SIDE_MODULE=1 exports all functions (needed for plugins)
    # -fexceptions must match main module for exception handling to work
    set(EMSCRIPTEN_SIDE_MODULE_FLAGS "-sSIDE_MODULE=1 -fexceptions")
endif()

macro(configure_plugin target)
    if(YMERY_WEB)
        set_target_properties(${target} PROPERTIES
            SUFFIX ".wasm"
            LINK_FLAGS "${EMSCRIPTEN_SIDE_MODULE_FLAGS}"
        )
        # Add include paths explicitly (needed since we clear LINK_LIBRARIES)
        target_include_directories(${target} PRIVATE
            ${yaml-cpp_SOURCE_DIR}/include
            ${spdlog_SOURCE_DIR}/include
        )
        # Clear link libraries - SIDE_MODULE imports symbols from MAIN_MODULE at runtime
        set_target_properties(${target} PROPERTIES LINK_LIBRARIES "")
    endif()
endmacro()

# Shared implementation as object library
add_library(filesystem_common OBJECT common.cpp)
target_include_directories(filesystem_common PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(filesystem_common PRIVATE spdlog::spdlog)

# Tree-like plugin (for direct use: data-tree: filesystem)
add_library(filesystem SHARED tree.cpp $<TARGET_OBJECTS:filesystem_common>)
target_link_libraries(filesystem PRIVATE ymery_lib spdlog::spdlog)
target_include_directories(filesystem PRIVATE ${CMAKE_SOURCE_DIR}/src)
set_target_properties(filesystem PROPERTIES
    PREFIX ""
    OUTPUT_NAME "filesystem"
    LIBRARY_OUTPUT_DIRECTORY "${PLUGIN_OUTPUT_DIR}"
    LIBRARY_OUTPUT_DIRECTORY_DEBUG "${PLUGIN_OUTPUT_DIR}"
    LIBRARY_OUTPUT_DIRECTORY_RELEASE "${PLUGIN_OUTPUT_DIR}"
    LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO "${PLUGIN_OUTPUT_DIR}"
    LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL "${PLUGIN_OUTPUT_DIR}"
)
configure_plugin(filesystem)

# Device-manager plugin (for kernel: /providers/filesystem)
add_library(filesystem-provider SHARED device.cpp $<TARGET_OBJECTS:filesystem_common>)
target_link_libraries(filesystem-provider PRIVATE ymery_lib spdlog::spdlog)
target_include_directories(filesystem-provider PRIVATE ${CMAKE_SOURCE_DIR}/src)
set_target_properties(filesystem-provider PROPERTIES
    PREFIX ""
    OUTPUT_NAME "filesystem-provider"
    LIBRARY_OUTPUT_DIRECTORY "${PLUGIN_OUTPUT_DIR}"
    LIBRARY_OUTPUT_DIRECTORY_DEBUG "${PLUGIN_OUTPUT_DIR}"
    LIBRARY_OUTPUT_DIRECTORY_RELEASE "${PLUGIN_OUTPUT_DIR}"
    LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO "${PLUGIN_OUTPUT_DIR}"
    LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL "${PLUGIN_OUTPUT_DIR}"
)
configure_plugin(filesystem-provider)
