# editor plugin - layout editor widgets (widget-tree, editor, preview, properties, code-preview, data-editor)

add_library(editor_plugin SHARED main.cpp)

target_link_libraries(editor_plugin PRIVATE
    ymery_lib
    imgui_color_text_edit
    spdlog::spdlog
)

target_include_directories(editor_plugin PRIVATE ${CMAKE_SOURCE_DIR}/src)

set_target_properties(editor_plugin PROPERTIES
    PREFIX ""
    OUTPUT_NAME "editor"
    LIBRARY_OUTPUT_DIRECTORY "${PLUGIN_OUTPUT_DIR}"
    LIBRARY_OUTPUT_DIRECTORY_DEBUG "${PLUGIN_OUTPUT_DIR}"
    LIBRARY_OUTPUT_DIRECTORY_RELEASE "${PLUGIN_OUTPUT_DIR}"
    LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO "${PLUGIN_OUTPUT_DIR}"
    LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL "${PLUGIN_OUTPUT_DIR}"
)

configure_plugin(editor_plugin)
