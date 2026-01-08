# imspinner plugin - Loading spinners from imspinner library
# This plugin provides: spinner

# Plugin target
add_library(imspinner_plugin SHARED main.cpp)
target_link_libraries(imspinner_plugin PRIVATE ymery_lib)
target_include_directories(imspinner_plugin PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${imspinner_SOURCE_DIR}
)

# Configure output - plugin name is "imspinner"
set_target_properties(imspinner_plugin PROPERTIES
    PREFIX ""
    OUTPUT_NAME "imspinner"
    LIBRARY_OUTPUT_DIRECTORY "${PLUGIN_OUTPUT_DIR}"
    LIBRARY_OUTPUT_DIRECTORY_DEBUG "${PLUGIN_OUTPUT_DIR}"
    LIBRARY_OUTPUT_DIRECTORY_RELEASE "${PLUGIN_OUTPUT_DIR}"
    LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO "${PLUGIN_OUTPUT_DIR}"
    LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL "${PLUGIN_OUTPUT_DIR}"
    ARCHIVE_OUTPUT_DIRECTORY "${PLUGIN_OUTPUT_DIR}"
    ARCHIVE_OUTPUT_DIRECTORY_DEBUG "${PLUGIN_OUTPUT_DIR}"
    ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${PLUGIN_OUTPUT_DIR}"
    ARCHIVE_OUTPUT_DIRECTORY_RELWITHDEBINFO "${PLUGIN_OUTPUT_DIR}"
    ARCHIVE_OUTPUT_DIRECTORY_MINSIZEREL "${PLUGIN_OUTPUT_DIR}"
)

# Configure for Emscripten if building for web
configure_plugin(imspinner_plugin)
