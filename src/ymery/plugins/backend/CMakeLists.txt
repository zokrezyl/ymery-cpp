# Build each tree-like backend as a shared library plugin

# On Android, output to the default location so Gradle bundles them in APK
if(YMERY_ANDROID)
    set(PLUGIN_OUTPUT_DIR "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
else()
    set(PLUGIN_OUTPUT_DIR "${CMAKE_BINARY_DIR}/plugins")
endif()

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
        LIBRARY_OUTPUT_DIRECTORY "${PLUGIN_OUTPUT_DIR}"
        LIBRARY_OUTPUT_DIRECTORY_DEBUG "${PLUGIN_OUTPUT_DIR}"
        LIBRARY_OUTPUT_DIRECTORY_RELEASE "${PLUGIN_OUTPUT_DIR}"
        LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO "${PLUGIN_OUTPUT_DIR}"
        LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL "${PLUGIN_OUTPUT_DIR}"
    )
endforeach()

# Waveform plugin (requires threading)
add_library(waveform SHARED waveform.cpp)
target_link_libraries(waveform PRIVATE ymery)
target_include_directories(waveform PRIVATE ${CMAKE_SOURCE_DIR}/src)
set_target_properties(waveform PROPERTIES
    PREFIX ""
    OUTPUT_NAME "waveform"
    LIBRARY_OUTPUT_DIRECTORY "${PLUGIN_OUTPUT_DIR}"
    LIBRARY_OUTPUT_DIRECTORY_DEBUG "${PLUGIN_OUTPUT_DIR}"
    LIBRARY_OUTPUT_DIRECTORY_RELEASE "${PLUGIN_OUTPUT_DIR}"
    LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO "${PLUGIN_OUTPUT_DIR}"
    LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL "${PLUGIN_OUTPUT_DIR}"
)
