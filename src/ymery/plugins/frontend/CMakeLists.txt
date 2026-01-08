# Frontend plugins - one plugin per GitHub source repository or feature area

# Plugin output directory
if(DEFINED YMERY_PLUGIN_DIR)
    set(PLUGIN_OUTPUT_DIR "${YMERY_PLUGIN_DIR}")
elseif(YMERY_ANDROID)
    set(PLUGIN_OUTPUT_DIR "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
else()
    set(PLUGIN_OUTPUT_DIR "${CMAKE_BINARY_DIR}/plugins")
endif()

# For Emscripten: compile plugins as side modules
if(YMERY_WEB)
    set(EMSCRIPTEN_SIDE_MODULE_FLAGS "-sSIDE_MODULE=1 -fexceptions")
endif()

macro(configure_plugin target)
    if(YMERY_WEB)
        set_target_properties(${target} PROPERTIES
            SUFFIX ".wasm"
            LINK_FLAGS "${EMSCRIPTEN_SIDE_MODULE_FLAGS}"
        )
    endif()
endmacro()

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

# =============================================================================
# Consolidated plugins - one plugin per GitHub source repository
# =============================================================================
add_subdirectory(imgui)           # ocornut/imgui widgets
add_subdirectory(implot)          # epezent/implot
add_subdirectory(imspinner)       # dalerank/imspinner
add_subdirectory(imgui-knobs)     # altschuler/imgui-knobs
add_subdirectory(imgui-toggle)    # cmdwtf/imgui_toggle
add_subdirectory(imcoolbar)       # aiekick/ImCoolBar
add_subdirectory(implot3d)        # brenocq/implot3d
add_subdirectory(imnodes)         # Nelarius/imnodes
add_subdirectory(im-anim)         # soufianekhiat/ImAnim
add_subdirectory(imguizmo)        # CedricGuillemet/ImGuizmo
add_subdirectory(imgui-markdown)  # juliettef/imgui_markdown

# =============================================================================
# Feature-area plugins
# =============================================================================
add_subdirectory(editor)          # Layout editor widgets
add_subdirectory(debug)           # Debug/logging widgets

# =============================================================================
# Standalone plugins - single-purpose widgets
# =============================================================================
add_subdirectory(node-graph)      # Node graph widget (from gist)
add_subdirectory(nodes)           # Nodes widget (from gist)
add_subdirectory(hex-editor)      # Hex editor widget
add_subdirectory(piano)           # Piano keyboard widget
add_subdirectory(shader-toy)      # WebGPU shader playground (POC)

# Copy meta.yaml files to build directory
file(GLOB META_YAML_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.meta.yaml")
foreach(meta_file ${META_YAML_FILES})
    get_filename_component(meta_name ${meta_file} NAME)
    configure_file(${meta_file} "${CMAKE_BINARY_DIR}/plugins/${meta_name}" COPYONLY)
endforeach()
