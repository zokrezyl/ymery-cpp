# implot plugin - Plotting widgets from implot library
# This plugin provides: plot, subplots, line, scatter, bars, etc.

# Plugin target
add_library(implot_plugin SHARED main.cpp)
target_link_libraries(implot_plugin PRIVATE ymery_lib implot)
target_include_directories(implot_plugin PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${implot_SOURCE_DIR}
)

# Configure output - plugin name is "implot"
set_target_properties(implot_plugin PROPERTIES
    PREFIX ""
    OUTPUT_NAME "implot"
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
configure_plugin(implot_plugin)
