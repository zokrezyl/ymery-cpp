# Build each widget as a shared library plugin

set(WIDGET_PLUGINS
    text
    button
    popup
    popup-modal
    input-text
    input-int
    slider-int
    slider-float
    checkbox
    combo
    color-edit
    separator
    spacing
    window
    column
    row
    group
    tree-node
    collapsing-header
    tab-bar
    tab-item
    child
)

foreach(plugin ${WIDGET_PLUGINS})
    add_library(${plugin} SHARED ${plugin}.cpp)
    target_link_libraries(${plugin} PRIVATE ymery imgui)
    target_include_directories(${plugin} PRIVATE ${CMAKE_SOURCE_DIR}/src)
    set_target_properties(${plugin} PROPERTIES
        PREFIX ""
        OUTPUT_NAME "${plugin}"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins/frontend"
    )
endforeach()

# Markdown widget (requires imgui_md)
add_library(markdown SHARED markdown.cpp)
target_link_libraries(markdown PRIVATE ymery imgui imgui_md)
target_include_directories(markdown PRIVATE ${CMAKE_SOURCE_DIR}/src)
set_target_properties(markdown PROPERTIES
    PREFIX ""
    OUTPUT_NAME "markdown"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins/frontend"
)

# ImPlot widgets (require implot) - use _plugin suffix to avoid conflict with library
set(IMPLOT_PLUGINS implot implot-group)
foreach(plugin ${IMPLOT_PLUGINS})
    add_library(${plugin}_plugin SHARED ${plugin}.cpp)
    target_link_libraries(${plugin}_plugin PRIVATE ymery imgui implot)
    target_include_directories(${plugin}_plugin PRIVATE ${CMAKE_SOURCE_DIR}/src)
    set_target_properties(${plugin}_plugin PROPERTIES
        PREFIX ""
        OUTPUT_NAME "${plugin}"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins/frontend"
    )
endforeach()

# Node editor widgets (require imgui_node_editor)
set(NODE_EDITOR_PLUGINS node-editor node node-pin)
foreach(plugin ${NODE_EDITOR_PLUGINS})
    add_library(${plugin} SHARED ${plugin}.cpp)
    target_link_libraries(${plugin} PRIVATE ymery imgui imgui_node_editor)
    target_include_directories(${plugin} PRIVATE ${CMAKE_SOURCE_DIR}/src)
    set_target_properties(${plugin} PROPERTIES
        PREFIX ""
        OUTPUT_NAME "${plugin}"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins/frontend"
    )
endforeach()
