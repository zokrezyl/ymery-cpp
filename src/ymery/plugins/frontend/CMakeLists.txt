# Build each widget as a shared library plugin

set(WIDGET_PLUGINS
    text
    button
    popup
    popup-modal
    tooltip
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
set(IMPLOT_PLUGINS implot implot-group implot-layer)
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

# Node editor widgets - DISABLED due to imgui_node_editor compatibility issues
# set(NODE_EDITOR_PLUGINS node-editor node node-pin)
# foreach(plugin ${NODE_EDITOR_PLUGINS})
#     add_library(${plugin} SHARED ${plugin}.cpp)
#     target_link_libraries(${plugin} PRIVATE ymery imgui imgui_node_editor)
#     target_include_directories(${plugin} PRIVATE ${CMAKE_SOURCE_DIR}/src)
#     set_target_properties(${plugin} PROPERTIES
#         PREFIX ""
#         OUTPUT_NAME "${plugin}"
#         LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins/frontend"
#     )
# endforeach()

# Knob widget (requires imgui_knobs)
add_library(knob SHARED knob.cpp)
target_link_libraries(knob PRIVATE ymery imgui imgui_knobs)
target_include_directories(knob PRIVATE ${CMAKE_SOURCE_DIR}/src)
set_target_properties(knob PROPERTIES
    PREFIX ""
    OUTPUT_NAME "knob"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins/frontend"
)

# Spinner widget (requires imspinner - header only)
add_library(spinner SHARED spinner.cpp)
target_link_libraries(spinner PRIVATE ymery imgui imspinner)
target_include_directories(spinner PRIVATE ${CMAKE_SOURCE_DIR}/src)
set_target_properties(spinner PROPERTIES
    PREFIX ""
    OUTPUT_NAME "spinner"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins/frontend"
)

# Spinners demo widget (requires imspinner - header only)
add_library(spinners_demo SHARED spinners_demo.cpp)
target_link_libraries(spinners_demo PRIVATE ymery imgui imspinner)
target_compile_definitions(spinners_demo PRIVATE IMSPINNER_DEMO)
target_include_directories(spinners_demo PRIVATE ${CMAKE_SOURCE_DIR}/src)
set_target_properties(spinners_demo PROPERTIES
    PREFIX ""
    OUTPUT_NAME "spinners-demo"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins/frontend"
)

# Toggle widget (requires imgui_toggle)
add_library(toggle SHARED toggle.cpp)
target_link_libraries(toggle PRIVATE ymery imgui imgui_toggle)
target_include_directories(toggle PRIVATE ${CMAKE_SOURCE_DIR}/src)
set_target_properties(toggle PROPERTIES
    PREFIX ""
    OUTPUT_NAME "toggle"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins/frontend"
)

# Coolbar widget (requires imcoolbar)
add_library(coolbar SHARED coolbar.cpp)
target_link_libraries(coolbar PRIVATE ymery imgui imcoolbar)
target_include_directories(coolbar PRIVATE ${CMAKE_SOURCE_DIR}/src)
set_target_properties(coolbar PROPERTIES
    PREFIX ""
    OUTPUT_NAME "coolbar"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins/frontend"
)

# Gizmo widget (requires imguizmo)
add_library(gizmo SHARED gizmo.cpp)
target_link_libraries(gizmo PRIVATE ymery imgui imguizmo)
target_include_directories(gizmo PRIVATE ${CMAKE_SOURCE_DIR}/src)
set_target_properties(gizmo PROPERTIES
    PREFIX ""
    OUTPUT_NAME "gizmo"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins/frontend"
)

# Plot3D widget (requires implot3d)
add_library(plot3d SHARED plot3d.cpp)
target_link_libraries(plot3d PRIVATE ymery imgui implot3d)
target_include_directories(plot3d PRIVATE ${CMAKE_SOURCE_DIR}/src)
set_target_properties(plot3d PROPERTIES
    PREFIX ""
    OUTPUT_NAME "plot3d"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins/frontend"
)

# Copy meta.yaml files to build directory
file(GLOB META_YAML_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.meta.yaml")
foreach(meta_file ${META_YAML_FILES})
    get_filename_component(meta_name ${meta_file} NAME)
    configure_file(${meta_file} "${CMAKE_BINARY_DIR}/plugins/frontend/${meta_name}" COPYONLY)
endforeach()
