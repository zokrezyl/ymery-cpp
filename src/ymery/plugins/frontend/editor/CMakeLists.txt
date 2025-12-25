# Editor widget plugins

# editor-widget-tree
add_library(editor-widget-tree SHARED widget-tree.cpp)
target_link_libraries(editor-widget-tree PRIVATE ymery imgui spdlog::spdlog)
set_target_properties(editor-widget-tree PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

# editor-canvas
add_library(editor-canvas SHARED canvas.cpp)
target_link_libraries(editor-canvas PRIVATE ymery imgui spdlog::spdlog)
set_target_properties(editor-canvas PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

# editor-properties
add_library(editor-properties SHARED properties.cpp)
target_link_libraries(editor-properties PRIVATE ymery imgui spdlog::spdlog)
set_target_properties(editor-properties PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)
