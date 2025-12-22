# Build each tree-like backend as a shared library plugin

set(BACKEND_PLUGINS
    simple-data-tree
    data-tree
)

foreach(plugin ${BACKEND_PLUGINS})
    add_library(${plugin} SHARED ${plugin}.cpp)
    target_link_libraries(${plugin} PRIVATE ymery)
    target_include_directories(${plugin} PRIVATE ${CMAKE_SOURCE_DIR}/src)
    set_target_properties(${plugin} PROPERTIES
        PREFIX ""
        OUTPUT_NAME "${plugin}"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins/backend"
    )
endforeach()

# Waveform plugin (requires threading)
add_library(waveform SHARED waveform.cpp)
target_link_libraries(waveform PRIVATE ymery)
target_include_directories(waveform PRIVATE ${CMAKE_SOURCE_DIR}/src)
set_target_properties(waveform PROPERTIES
    PREFIX ""
    OUTPUT_NAME "waveform"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins/backend"
)
