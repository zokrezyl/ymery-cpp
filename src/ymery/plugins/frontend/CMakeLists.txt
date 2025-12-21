# Build each widget as a shared library plugin

set(WIDGET_PLUGINS
    text
    button
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
