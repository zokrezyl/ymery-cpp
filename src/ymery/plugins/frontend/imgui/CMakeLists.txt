# imgui plugin - core ImGui widgets
# This plugin provides: button, text, checkbox, window, etc.

# Plugin target
add_library(imgui_plugin SHARED main.cpp)
target_link_libraries(imgui_plugin PRIVATE ymery_lib)
target_include_directories(imgui_plugin PRIVATE ${CMAKE_SOURCE_DIR}/src)

# Configure output - plugin name is "imgui"
set_target_properties(imgui_plugin PROPERTIES
    PREFIX ""
    OUTPUT_NAME "imgui"
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
configure_plugin(imgui_plugin)
