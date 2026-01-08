# debug plugin - debugging/logging widgets (logs-view, errors-view)

add_library(debug_plugin SHARED main.cpp)

target_link_libraries(debug_plugin PRIVATE
    ymery_lib
    spdlog::spdlog
)

target_include_directories(debug_plugin PRIVATE ${CMAKE_SOURCE_DIR}/src)

set_target_properties(debug_plugin PROPERTIES
    PREFIX ""
    OUTPUT_NAME "debug"
    LIBRARY_OUTPUT_DIRECTORY "${PLUGIN_OUTPUT_DIR}"
    LIBRARY_OUTPUT_DIRECTORY_DEBUG "${PLUGIN_OUTPUT_DIR}"
    LIBRARY_OUTPUT_DIRECTORY_RELEASE "${PLUGIN_OUTPUT_DIR}"
    LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO "${PLUGIN_OUTPUT_DIR}"
    LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL "${PLUGIN_OUTPUT_DIR}"
)

configure_plugin(debug_plugin)
