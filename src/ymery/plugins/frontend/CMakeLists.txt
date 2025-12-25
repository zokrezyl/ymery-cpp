# Build each widget as a shared library plugin

# On Android, output to the default location so Gradle bundles them in APK
if(YMERY_ANDROID)
    set(PLUGIN_OUTPUT_DIR "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
else()
    set(PLUGIN_OUTPUT_DIR "${CMAKE_BINARY_DIR}/plugins")
endif()

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
        LIBRARY_OUTPUT_DIRECTORY "${PLUGIN_OUTPUT_DIR}"
        LIBRARY_OUTPUT_DIRECTORY_DEBUG "${PLUGIN_OUTPUT_DIR}"
        LIBRARY_OUTPUT_DIRECTORY_RELEASE "${PLUGIN_OUTPUT_DIR}"
        LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO "${PLUGIN_OUTPUT_DIR}"
        LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL "${PLUGIN_OUTPUT_DIR}"
    )
endforeach()

# Markdown widget (requires imgui_md)
add_library(markdown SHARED markdown.cpp)
target_link_libraries(markdown PRIVATE ymery imgui imgui_md)
target_include_directories(markdown PRIVATE ${CMAKE_SOURCE_DIR}/src)
set_target_properties(markdown PROPERTIES
    PREFIX ""
    OUTPUT_NAME "markdown"
    LIBRARY_OUTPUT_DIRECTORY "${PLUGIN_OUTPUT_DIR}"
    LIBRARY_OUTPUT_DIRECTORY_DEBUG "${PLUGIN_OUTPUT_DIR}"
    LIBRARY_OUTPUT_DIRECTORY_RELEASE "${PLUGIN_OUTPUT_DIR}"
    LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO "${PLUGIN_OUTPUT_DIR}"
    LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL "${PLUGIN_OUTPUT_DIR}"
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
        LIBRARY_OUTPUT_DIRECTORY "${PLUGIN_OUTPUT_DIR}"
        LIBRARY_OUTPUT_DIRECTORY_DEBUG "${PLUGIN_OUTPUT_DIR}"
        LIBRARY_OUTPUT_DIRECTORY_RELEASE "${PLUGIN_OUTPUT_DIR}"
        LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO "${PLUGIN_OUTPUT_DIR}"
        LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL "${PLUGIN_OUTPUT_DIR}"
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
#         LIBRARY_OUTPUT_DIRECTORY "${PLUGIN_OUTPUT_DIR}"
#     )
# endforeach()

# Helper macro for setting plugin output directories
macro(set_plugin_output_dirs target)
    set_target_properties(${target} PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY "${PLUGIN_OUTPUT_DIR}"
        LIBRARY_OUTPUT_DIRECTORY_DEBUG "${PLUGIN_OUTPUT_DIR}"
        LIBRARY_OUTPUT_DIRECTORY_RELEASE "${PLUGIN_OUTPUT_DIR}"
        LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO "${PLUGIN_OUTPUT_DIR}"
        LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL "${PLUGIN_OUTPUT_DIR}"
    )
endmacro()

# Knob widget (requires imgui_knobs)
add_library(knob SHARED knob.cpp)
target_link_libraries(knob PRIVATE ymery imgui imgui_knobs)
target_include_directories(knob PRIVATE ${CMAKE_SOURCE_DIR}/src)
set_target_properties(knob PROPERTIES PREFIX "" OUTPUT_NAME "knob")
set_plugin_output_dirs(knob)

# Spinner widget (requires imspinner - header only)
add_library(spinner SHARED spinner.cpp)
target_link_libraries(spinner PRIVATE ymery imgui imspinner)
target_include_directories(spinner PRIVATE ${CMAKE_SOURCE_DIR}/src)
set_target_properties(spinner PROPERTIES PREFIX "" OUTPUT_NAME "spinner")
set_plugin_output_dirs(spinner)

# Spinners demo widget (requires imspinner - header only)
add_library(spinners_demo SHARED spinners_demo.cpp)
target_link_libraries(spinners_demo PRIVATE ymery imgui imspinner)
target_compile_definitions(spinners_demo PRIVATE IMSPINNER_DEMO)
target_include_directories(spinners_demo PRIVATE ${CMAKE_SOURCE_DIR}/src)
set_target_properties(spinners_demo PROPERTIES PREFIX "" OUTPUT_NAME "spinners-demo")
set_plugin_output_dirs(spinners_demo)

# Toggle widget (requires imgui_toggle)
add_library(toggle SHARED toggle.cpp)
target_link_libraries(toggle PRIVATE ymery imgui imgui_toggle)
target_include_directories(toggle PRIVATE ${CMAKE_SOURCE_DIR}/src)
set_target_properties(toggle PROPERTIES PREFIX "" OUTPUT_NAME "toggle")
set_plugin_output_dirs(toggle)

# Coolbar widget (requires imcoolbar)
add_library(coolbar SHARED coolbar.cpp)
target_link_libraries(coolbar PRIVATE ymery imgui imcoolbar)
target_include_directories(coolbar PRIVATE ${CMAKE_SOURCE_DIR}/src)
set_target_properties(coolbar PROPERTIES PREFIX "" OUTPUT_NAME "coolbar")
set_plugin_output_dirs(coolbar)

# Gizmo widget (requires imguizmo)
add_library(gizmo SHARED gizmo.cpp)
target_link_libraries(gizmo PRIVATE ymery imgui imguizmo)
target_include_directories(gizmo PRIVATE ${CMAKE_SOURCE_DIR}/src)
set_target_properties(gizmo PROPERTIES PREFIX "" OUTPUT_NAME "gizmo")
set_plugin_output_dirs(gizmo)

# Plot3D widget (requires implot3d)
add_library(plot3d SHARED plot3d.cpp)
target_link_libraries(plot3d PRIVATE ymery imgui implot3d)
target_include_directories(plot3d PRIVATE ${CMAKE_SOURCE_DIR}/src)
set_target_properties(plot3d PROPERTIES PREFIX "" OUTPUT_NAME "plot3d")
set_plugin_output_dirs(plot3d)

# Editor widget plugins
add_library(editor-widget-tree SHARED editor/widget-tree.cpp)
target_link_libraries(editor-widget-tree PRIVATE ymery imgui spdlog::spdlog)
target_include_directories(editor-widget-tree PRIVATE ${CMAKE_SOURCE_DIR}/src)
set_target_properties(editor-widget-tree PROPERTIES PREFIX "" OUTPUT_NAME "editor-widget-tree")
set_plugin_output_dirs(editor-widget-tree)

add_library(editor-canvas SHARED editor/canvas.cpp)
target_link_libraries(editor-canvas PRIVATE ymery imgui spdlog::spdlog)
target_include_directories(editor-canvas PRIVATE ${CMAKE_SOURCE_DIR}/src)
set_target_properties(editor-canvas PROPERTIES PREFIX "" OUTPUT_NAME "editor-canvas")
set_plugin_output_dirs(editor-canvas)

add_library(editor-properties SHARED editor/properties.cpp)
target_link_libraries(editor-properties PRIVATE ymery imgui spdlog::spdlog)
target_include_directories(editor-properties PRIVATE ${CMAKE_SOURCE_DIR}/src)
set_target_properties(editor-properties PROPERTIES PREFIX "" OUTPUT_NAME "editor-properties")
set_plugin_output_dirs(editor-properties)

add_library(editor-preview SHARED editor/preview.cpp)
target_link_libraries(editor-preview PRIVATE ymery imgui spdlog::spdlog)
target_include_directories(editor-preview PRIVATE ${CMAKE_SOURCE_DIR}/src)
set_target_properties(editor-preview PROPERTIES PREFIX "" OUTPUT_NAME "editor-preview")
set_plugin_output_dirs(editor-preview)

# Docking widgets
add_library(docking-main-window SHARED docking-main-window.cpp)
target_link_libraries(docking-main-window PRIVATE ymery imgui)
target_include_directories(docking-main-window PRIVATE ${CMAKE_SOURCE_DIR}/src)
set_target_properties(docking-main-window PROPERTIES PREFIX "" OUTPUT_NAME "docking-main-window")
set_plugin_output_dirs(docking-main-window)

add_library(docking-split SHARED docking-split.cpp)
target_link_libraries(docking-split PRIVATE ymery imgui)
target_include_directories(docking-split PRIVATE ${CMAKE_SOURCE_DIR}/src)
set_target_properties(docking-split PROPERTIES PREFIX "" OUTPUT_NAME "docking-split")
set_plugin_output_dirs(docking-split)

add_library(dockable-window SHARED dockable-window.cpp)
target_link_libraries(dockable-window PRIVATE ymery imgui)
target_include_directories(dockable-window PRIVATE ${CMAKE_SOURCE_DIR}/src)
set_target_properties(dockable-window PROPERTIES PREFIX "" OUTPUT_NAME "dockable-window")
set_plugin_output_dirs(dockable-window)

# Copy meta.yaml files to build directory
file(GLOB META_YAML_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.meta.yaml")
foreach(meta_file ${META_YAML_FILES})
    get_filename_component(meta_name ${meta_file} NAME)
    configure_file(${meta_file} "${CMAKE_BINARY_DIR}/plugins/${meta_name}" COPYONLY)
endforeach()
