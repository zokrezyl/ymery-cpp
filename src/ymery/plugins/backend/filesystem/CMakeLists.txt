# Filesystem plugin - exposes both tree-like and device-manager interfaces

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
