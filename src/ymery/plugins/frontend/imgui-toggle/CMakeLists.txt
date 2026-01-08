# imgui-toggle plugin - Toggle switch controls
add_library(imgui_toggle_plugin SHARED main.cpp)
target_link_libraries(imgui_toggle_plugin PRIVATE ymery_lib imgui_toggle)
target_include_directories(imgui_toggle_plugin PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${imgui_toggle_SOURCE_DIR}
)

set_target_properties(imgui_toggle_plugin PROPERTIES
    PREFIX ""
    OUTPUT_NAME "imgui-toggle"
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

configure_plugin(imgui_toggle_plugin)
