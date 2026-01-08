# imgui-node-editor plugin - thedmd/imgui-node-editor
set(PLUGIN_NAME imgui-node-editor)

# Pull in imgui-node-editor dependency
CPMAddPackage(
    NAME imgui_node_editor
    GITHUB_REPOSITORY thedmd/imgui-node-editor
    GIT_TAG master
    DOWNLOAD_ONLY YES
)

if(imgui_node_editor_ADDED)
    file(GLOB IMGUI_NODE_EDITOR_SOURCES ${imgui_node_editor_SOURCE_DIR}/*.cpp)
    add_library(imgui_node_editor_lib STATIC ${IMGUI_NODE_EDITOR_SOURCES})
    target_include_directories(imgui_node_editor_lib PUBLIC ${imgui_node_editor_SOURCE_DIR})
    target_link_libraries(imgui_node_editor_lib PUBLIC imgui)
endif()

add_library(${PLUGIN_NAME} SHARED main.cpp)
target_link_libraries(${PLUGIN_NAME} PRIVATE ymery_lib imgui imgui_node_editor_lib)
target_include_directories(${PLUGIN_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${imgui_node_editor_SOURCE_DIR}
)
set_target_properties(${PLUGIN_NAME} PROPERTIES PREFIX "" OUTPUT_NAME "${PLUGIN_NAME}")
set_plugin_output_dirs(${PLUGIN_NAME})
