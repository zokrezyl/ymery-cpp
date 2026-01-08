# imgui-markdown plugin - markdown rendering widget

add_library(imgui_markdown_plugin SHARED main.cpp)

target_link_libraries(imgui_markdown_plugin PRIVATE
    ymery_lib
    imgui_md
)

target_include_directories(imgui_markdown_plugin PRIVATE ${CMAKE_SOURCE_DIR}/src)

set_target_properties(imgui_markdown_plugin PROPERTIES
    PREFIX ""
    OUTPUT_NAME "imgui-markdown"
    LIBRARY_OUTPUT_DIRECTORY "${PLUGIN_OUTPUT_DIR}"
    LIBRARY_OUTPUT_DIRECTORY_DEBUG "${PLUGIN_OUTPUT_DIR}"
    LIBRARY_OUTPUT_DIRECTORY_RELEASE "${PLUGIN_OUTPUT_DIR}"
    LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO "${PLUGIN_OUTPUT_DIR}"
    LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL "${PLUGIN_OUTPUT_DIR}"
)

configure_plugin(imgui_markdown_plugin)
