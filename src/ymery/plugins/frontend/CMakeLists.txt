# Build each widget as a shared library plugin

# Plugin output directory - can be overridden by parent project via YMERY_PLUGIN_DIR
if(DEFINED YMERY_PLUGIN_DIR)
    set(PLUGIN_OUTPUT_DIR "${YMERY_PLUGIN_DIR}")
elseif(YMERY_ANDROID)
    # On Android, output to the default location so Gradle bundles them in APK
    set(PLUGIN_OUTPUT_DIR "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
else()
    set(PLUGIN_OUTPUT_DIR "${CMAKE_BINARY_DIR}/plugins")
endif()

# For Emscripten: compile plugins as side modules for dynamic linking
if(YMERY_WEB)
    # SIDE_MODULE=1 exports all functions (needed for plugins)
    # -fexceptions must match main module for exception handling to work
    set(EMSCRIPTEN_SIDE_MODULE_FLAGS "-sSIDE_MODULE=1 -fexceptions")
endif()

# Helper macro to configure a plugin target (handles Emscripten side module flags)
macro(configure_plugin target)
    if(YMERY_WEB)
        # Side modules import symbols from main module at runtime
        # But they still need include paths for compilation
        set_target_properties(${target} PROPERTIES
            SUFFIX ".wasm"
            LINK_FLAGS "${EMSCRIPTEN_SIDE_MODULE_FLAGS}"
        )
        # Add include paths explicitly (needed since we clear LINK_LIBRARIES)
        target_include_directories(${target} PRIVATE
            ${yaml-cpp_SOURCE_DIR}/include
            ${imgui_SOURCE_DIR}
            ${imgui_SOURCE_DIR}/backends
            ${implot_SOURCE_DIR}
            ${spdlog_SOURCE_DIR}/include
            ${imgui_markdown_SOURCE_DIR}
            ${imgui_knobs_SOURCE_DIR}
            ${imspinner_SOURCE_DIR}
            ${imgui_toggle_SOURCE_DIR}
            ${imcoolbar_SOURCE_DIR}
            ${imguizmo_SOURCE_DIR}
            ${implot3d_SOURCE_DIR}
            ${imgui_color_text_edit_SOURCE_DIR}
            ${imnodes_SOURCE_DIR}
        )
        # Clear link libraries - SIDE_MODULE imports symbols from MAIN_MODULE at runtime
        set_target_properties(${target} PROPERTIES LINK_LIBRARIES "")
    endif()
endmacro()

set(WIDGET_PLUGINS
    text
    button
    popup
    popup-modal
    tooltip
    input-text
    input-int
    input-float
    slider-int
    slider-float
    drag-int
    drag-float
    checkbox
    combo
    color-edit
    color-button
    separator
    separator-text
    spacing
    bullet-text
    window
    column
    row
    group
    tree-node
    collapsing-header
    tab-bar
    tab-item
    child
    menu-bar
    main-menu-bar
    menu
    menu-item
    same-line
    listbox
    selectable
    radio-button
    indent
    table
    table-row
    table-column
    imgui-main-window
    hello-imgui-main-window
    hello-imgui-menu
    hello-imgui-app-menu-items
    im-anim
)

foreach(plugin ${WIDGET_PLUGINS})
    add_library(${plugin} SHARED ${plugin}.cpp)
    target_link_libraries(${plugin} PRIVATE ymery_lib imgui)
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
    configure_plugin(${plugin})
endforeach()

# Markdown widget (requires imgui_md)
add_library(markdown SHARED markdown.cpp)
target_link_libraries(markdown PRIVATE ymery_lib imgui imgui_md)
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
configure_plugin(markdown)

# ImPlot widgets (require implot) - use _plugin suffix to avoid conflict with library
set(IMPLOT_PLUGINS implot implot-group implot-layer)
foreach(plugin ${IMPLOT_PLUGINS})
    add_library(${plugin}_plugin SHARED ${plugin}.cpp)
    target_link_libraries(${plugin}_plugin PRIVATE ymery_lib imgui implot)
    target_include_directories(${plugin}_plugin PRIVATE ${CMAKE_SOURCE_DIR}/src)
    set_target_properties(${plugin}_plugin PROPERTIES
        PREFIX ""
        OUTPUT_NAME "${plugin}"
        LIBRARY_OUTPUT_DIRECTORY "${PLUGIN_OUTPUT_DIR}"
        LIBRARY_OUTPUT_DIRECTORY_DEBUG "${PLUGIN_OUTPUT_DIR}"
        LIBRARY_OUTPUT_DIRECTORY_RELEASE "${PLUGIN_OUTPUT_DIR}"
        LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO "${PLUGIN_OUTPUT_DIR}"
        LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL "${PLUGIN_OUTPUT_DIR}"
        # Put Windows import libs in plugins/ to avoid conflict with library import libs
        ARCHIVE_OUTPUT_DIRECTORY "${PLUGIN_OUTPUT_DIR}"
        ARCHIVE_OUTPUT_DIRECTORY_DEBUG "${PLUGIN_OUTPUT_DIR}"
        ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${PLUGIN_OUTPUT_DIR}"
        ARCHIVE_OUTPUT_DIRECTORY_RELWITHDEBINFO "${PLUGIN_OUTPUT_DIR}"
        ARCHIVE_OUTPUT_DIRECTORY_MINSIZEREL "${PLUGIN_OUTPUT_DIR}"
    )
    configure_plugin(${plugin}_plugin)
endforeach()

# Node editor widgets - DISABLED (requires pthom's patched imgui)
# set(NODE_EDITOR_PLUGINS node-editor node node-pin)
# foreach(plugin ${NODE_EDITOR_PLUGINS})
#     add_library(${plugin} SHARED ${plugin}.cpp)
#     target_link_libraries(${plugin} PRIVATE ymery imgui imgui_node_editor)
#     target_include_directories(${plugin} PRIVATE ${CMAKE_SOURCE_DIR}/src)
#     set_target_properties(${plugin} PROPERTIES
#         PREFIX ""
#         OUTPUT_NAME "${plugin}"
#         LIBRARY_OUTPUT_DIRECTORY "${PLUGIN_OUTPUT_DIR}"
#         LIBRARY_OUTPUT_DIRECTORY_DEBUG "${PLUGIN_OUTPUT_DIR}"
#         LIBRARY_OUTPUT_DIRECTORY_RELEASE "${PLUGIN_OUTPUT_DIR}"
#         LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO "${PLUGIN_OUTPUT_DIR}"
#         LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL "${PLUGIN_OUTPUT_DIR}"
#     )
# endforeach()

# Helper macro for setting plugin output directories and web config
macro(set_plugin_output_dirs target)
    set_target_properties(${target} PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY "${PLUGIN_OUTPUT_DIR}"
        LIBRARY_OUTPUT_DIRECTORY_DEBUG "${PLUGIN_OUTPUT_DIR}"
        LIBRARY_OUTPUT_DIRECTORY_RELEASE "${PLUGIN_OUTPUT_DIR}"
        LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO "${PLUGIN_OUTPUT_DIR}"
        LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL "${PLUGIN_OUTPUT_DIR}"
    )
    configure_plugin(${target})
endmacro()

# Knob widget (requires imgui_knobs)
add_library(knob SHARED knob.cpp)
target_link_libraries(knob PRIVATE ymery_lib imgui imgui_knobs)
target_include_directories(knob PRIVATE ${CMAKE_SOURCE_DIR}/src)
set_target_properties(knob PROPERTIES PREFIX "" OUTPUT_NAME "knob")
set_plugin_output_dirs(knob)

# Knob-int widget (requires imgui_knobs)
add_library(knob-int SHARED knob-int.cpp)
target_link_libraries(knob-int PRIVATE ymery_lib imgui imgui_knobs)
target_include_directories(knob-int PRIVATE ${CMAKE_SOURCE_DIR}/src)
set_target_properties(knob-int PROPERTIES PREFIX "" OUTPUT_NAME "knob-int")
set_plugin_output_dirs(knob-int)

# Spinner widget (requires imspinner - header only)
add_library(spinner SHARED spinner.cpp)
target_link_libraries(spinner PRIVATE ymery_lib imgui imspinner)
target_include_directories(spinner PRIVATE ${CMAKE_SOURCE_DIR}/src)
set_target_properties(spinner PROPERTIES PREFIX "" OUTPUT_NAME "spinner")
set_plugin_output_dirs(spinner)

# Spinners demo widget (requires imspinner - header only)
add_library(spinners_demo SHARED spinners_demo.cpp)
target_link_libraries(spinners_demo PRIVATE ymery_lib imgui imspinner)
target_compile_definitions(spinners_demo PRIVATE IMSPINNER_DEMO)
target_include_directories(spinners_demo PRIVATE ${CMAKE_SOURCE_DIR}/src)
set_target_properties(spinners_demo PROPERTIES PREFIX "" OUTPUT_NAME "spinners-demo")
set_plugin_output_dirs(spinners_demo)

# Toggle widget (requires imgui_toggle)
add_library(toggle SHARED toggle.cpp)
target_link_libraries(toggle PRIVATE ymery_lib imgui imgui_toggle)
target_include_directories(toggle PRIVATE ${CMAKE_SOURCE_DIR}/src)
set_target_properties(toggle PROPERTIES PREFIX "" OUTPUT_NAME "toggle")
set_plugin_output_dirs(toggle)

# Coolbar widget (requires imcoolbar)
add_library(coolbar SHARED coolbar.cpp)
target_link_libraries(coolbar PRIVATE ymery_lib imgui imcoolbar)
target_include_directories(coolbar PRIVATE ${CMAKE_SOURCE_DIR}/src)
set_target_properties(coolbar PROPERTIES PREFIX "" OUTPUT_NAME "coolbar")
set_plugin_output_dirs(coolbar)

# Gizmo widget (requires imguizmo)
add_library(gizmo SHARED gizmo.cpp)
target_link_libraries(gizmo PRIVATE ymery_lib imgui imguizmo)
target_include_directories(gizmo PRIVATE ${CMAKE_SOURCE_DIR}/src)
set_target_properties(gizmo PROPERTIES PREFIX "" OUTPUT_NAME "gizmo")
set_plugin_output_dirs(gizmo)

# Plot3D widget (requires implot3d)
add_library(plot3d SHARED plot3d.cpp)
target_link_libraries(plot3d PRIVATE ymery_lib imgui implot3d)
target_include_directories(plot3d PRIVATE ${CMAKE_SOURCE_DIR}/src)
set_target_properties(plot3d PROPERTIES PREFIX "" OUTPUT_NAME "plot3d")
set_plugin_output_dirs(plot3d)

# Imnodes widget (requires imnodes - use _plugin suffix to avoid target name conflict)
add_library(imnodes_plugin SHARED imnodes.cpp)
target_link_libraries(imnodes_plugin PRIVATE ymery_lib imgui imnodes)
target_include_directories(imnodes_plugin PRIVATE ${CMAKE_SOURCE_DIR}/src)
set_target_properties(imnodes_plugin PROPERTIES
    PREFIX ""
    OUTPUT_NAME "imnodes"
    ARCHIVE_OUTPUT_DIRECTORY "${PLUGIN_OUTPUT_DIR}"
    ARCHIVE_OUTPUT_DIRECTORY_DEBUG "${PLUGIN_OUTPUT_DIR}"
    ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${PLUGIN_OUTPUT_DIR}"
    ARCHIVE_OUTPUT_DIRECTORY_RELWITHDEBINFO "${PLUGIN_OUTPUT_DIR}"
    ARCHIVE_OUTPUT_DIRECTORY_MINSIZEREL "${PLUGIN_OUTPUT_DIR}"
)
set_plugin_output_dirs(imnodes_plugin)


# NodeGraph widget (from gist)
add_library(node_graph_plugin SHARED node-graph.cpp)
target_link_libraries(node_graph_plugin PRIVATE ymery_lib imgui)
target_include_directories(node_graph_plugin PRIVATE ${CMAKE_SOURCE_DIR}/src)
set_target_properties(node_graph_plugin PROPERTIES
    PREFIX ""
    OUTPUT_NAME "node-graph"
    ARCHIVE_OUTPUT_DIRECTORY "${PLUGIN_OUTPUT_DIR}"
    ARCHIVE_OUTPUT_DIRECTORY_DEBUG "${PLUGIN_OUTPUT_DIR}"
    ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${PLUGIN_OUTPUT_DIR}"
    ARCHIVE_OUTPUT_DIRECTORY_RELWITHDEBINFO "${PLUGIN_OUTPUT_DIR}"
    ARCHIVE_OUTPUT_DIRECTORY_MINSIZEREL "${PLUGIN_OUTPUT_DIR}"
)
set_plugin_output_dirs(node_graph_plugin)

# Nodes widget (from gist)
add_library(nodes_plugin SHARED nodes.cpp)
target_link_libraries(nodes_plugin PRIVATE ymery_lib imgui)
target_include_directories(nodes_plugin PRIVATE ${CMAKE_SOURCE_DIR}/src)
set_target_properties(nodes_plugin PROPERTIES
    PREFIX ""
    OUTPUT_NAME "nodes"
    ARCHIVE_OUTPUT_DIRECTORY "${PLUGIN_OUTPUT_DIR}"
    ARCHIVE_OUTPUT_DIRECTORY_DEBUG "${PLUGIN_OUTPUT_DIR}"
    ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${PLUGIN_OUTPUT_DIR}"
    ARCHIVE_OUTPUT_DIRECTORY_RELWITHDEBINFO "${PLUGIN_OUTPUT_DIR}"
    ARCHIVE_OUTPUT_DIRECTORY_MINSIZEREL "${PLUGIN_OUTPUT_DIR}"
)
set_plugin_output_dirs(nodes_plugin)

# Editor widget plugins
add_library(editor-widget-tree SHARED editor/widget-tree.cpp)
target_link_libraries(editor-widget-tree PRIVATE ymery_lib imgui spdlog::spdlog)
target_include_directories(editor-widget-tree PRIVATE ${CMAKE_SOURCE_DIR}/src)
set_target_properties(editor-widget-tree PROPERTIES PREFIX "" OUTPUT_NAME "editor-widget-tree")
set_plugin_output_dirs(editor-widget-tree)

add_library(editor SHARED editor/editor.cpp)
target_link_libraries(editor PRIVATE ymery_lib imgui spdlog::spdlog)
target_include_directories(editor PRIVATE ${CMAKE_SOURCE_DIR}/src)
set_target_properties(editor PROPERTIES PREFIX "" OUTPUT_NAME "editor")
set_plugin_output_dirs(editor)

add_library(code-preview SHARED editor/code-preview.cpp)
target_link_libraries(code-preview PRIVATE ymery_lib imgui imgui_color_text_edit spdlog::spdlog)
target_include_directories(code-preview PRIVATE ${CMAKE_SOURCE_DIR}/src)
set_target_properties(code-preview PROPERTIES PREFIX "" OUTPUT_NAME "code-preview")
set_plugin_output_dirs(code-preview)

add_library(editor-properties SHARED editor/properties.cpp)
target_link_libraries(editor-properties PRIVATE ymery_lib imgui spdlog::spdlog)
target_include_directories(editor-properties PRIVATE ${CMAKE_SOURCE_DIR}/src)
set_target_properties(editor-properties PROPERTIES PREFIX "" OUTPUT_NAME "editor-properties")
set_plugin_output_dirs(editor-properties)

add_library(preview SHARED editor/preview.cpp)
target_link_libraries(preview PRIVATE ymery_lib imgui spdlog::spdlog)
target_include_directories(preview PRIVATE ${CMAKE_SOURCE_DIR}/src)
set_target_properties(preview PROPERTIES PREFIX "" OUTPUT_NAME "preview")
set_plugin_output_dirs(preview)

add_library(data-editor SHARED editor/data-editor.cpp)
target_link_libraries(data-editor PRIVATE ymery_lib imgui spdlog::spdlog)
target_include_directories(data-editor PRIVATE ${CMAKE_SOURCE_DIR}/src)
set_target_properties(data-editor PROPERTIES PREFIX "" OUTPUT_NAME "data-editor")
set_plugin_output_dirs(data-editor)

# Docking widgets
add_library(docking-main-window SHARED docking-main-window.cpp)
target_link_libraries(docking-main-window PRIVATE ymery_lib imgui)
target_include_directories(docking-main-window PRIVATE ${CMAKE_SOURCE_DIR}/src)
set_target_properties(docking-main-window PROPERTIES PREFIX "" OUTPUT_NAME "docking-main-window")
set_plugin_output_dirs(docking-main-window)

add_library(docking-split SHARED docking-split.cpp)
target_link_libraries(docking-split PRIVATE ymery_lib imgui)
target_include_directories(docking-split PRIVATE ${CMAKE_SOURCE_DIR}/src)
set_target_properties(docking-split PROPERTIES PREFIX "" OUTPUT_NAME "docking-split")
set_plugin_output_dirs(docking-split)

add_library(dockable-window SHARED dockable-window.cpp)
target_link_libraries(dockable-window PRIVATE ymery_lib imgui)
target_include_directories(dockable-window PRIVATE ${CMAKE_SOURCE_DIR}/src)
set_target_properties(dockable-window PROPERTIES PREFIX "" OUTPUT_NAME "dockable-window")
set_plugin_output_dirs(dockable-window)

# Piano widget
add_library(piano SHARED piano.cpp)
target_link_libraries(piano PRIVATE ymery_lib imgui)
target_include_directories(piano PRIVATE ${CMAKE_SOURCE_DIR}/src)
set_target_properties(piano PROPERTIES PREFIX "" OUTPUT_NAME "piano")
set_plugin_output_dirs(piano)

# Hex editor widget
add_library(hex-editor SHARED hex-editor.cpp)
target_link_libraries(hex-editor PRIVATE ymery_lib imgui)
target_include_directories(hex-editor PRIVATE ${CMAKE_SOURCE_DIR}/src)
set_target_properties(hex-editor PROPERTIES PREFIX "" OUTPUT_NAME "hex-editor")
set_plugin_output_dirs(hex-editor)

# Logs view widget (spdlog messages)
add_library(logs-view SHARED logs-view.cpp)
target_link_libraries(logs-view PRIVATE ymery_lib imgui spdlog::spdlog)
target_include_directories(logs-view PRIVATE ${CMAKE_SOURCE_DIR}/src)
set_target_properties(logs-view PROPERTIES PREFIX "" OUTPUT_NAME "logs-view")
set_plugin_output_dirs(logs-view)

# Errors view widget (Result Errors)
add_library(errors-view SHARED errors-view.cpp)
target_link_libraries(errors-view PRIVATE ymery_lib imgui spdlog::spdlog)
target_include_directories(errors-view PRIVATE ${CMAKE_SOURCE_DIR}/src)
set_target_properties(errors-view PROPERTIES PREFIX "" OUTPUT_NAME "errors-view")
set_plugin_output_dirs(errors-view)

# Shader-toy widget (WebGPU custom shader POC)
add_library(shader-toy SHARED shader-toy.cpp)
target_link_libraries(shader-toy PRIVATE ymery_lib imgui)
if(YMERY_USE_WEBGPU AND NOT YMERY_WEB)
    target_link_libraries(shader-toy PRIVATE webgpu)
endif()
target_include_directories(shader-toy PRIVATE ${CMAKE_SOURCE_DIR}/src)
set_target_properties(shader-toy PROPERTIES PREFIX "" OUTPUT_NAME "shader-toy")
set_plugin_output_dirs(shader-toy)

# Copy meta.yaml files to build directory
file(GLOB META_YAML_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.meta.yaml")
foreach(meta_file ${META_YAML_FILES})
    get_filename_component(meta_name ${meta_file} NAME)
    configure_file(${meta_file} "${CMAKE_BINARY_DIR}/plugins/${meta_name}" COPYONLY)
endforeach()
