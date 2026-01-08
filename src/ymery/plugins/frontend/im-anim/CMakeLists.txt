# im-anim plugin - Animation demos using ImAnim library
set(PLUGIN_NAME im-anim_plugin)

add_library(${PLUGIN_NAME} SHARED
    main.cpp
)

target_include_directories(${PLUGIN_NAME} PRIVATE
    ${imanim_SOURCE_DIR}
    ${imgui_SOURCE_DIR}
)

target_link_libraries(${PLUGIN_NAME} PRIVATE
    ymery_lib
    im-anim
)

set_target_properties(${PLUGIN_NAME} PROPERTIES
    PREFIX ""
    OUTPUT_NAME "im-anim"
    LIBRARY_OUTPUT_DIRECTORY "${PLUGIN_OUTPUT_DIR}"
    LIBRARY_OUTPUT_DIRECTORY_DEBUG "${PLUGIN_OUTPUT_DIR}"
    LIBRARY_OUTPUT_DIRECTORY_RELEASE "${PLUGIN_OUTPUT_DIR}"
    LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO "${PLUGIN_OUTPUT_DIR}"
    LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL "${PLUGIN_OUTPUT_DIR}"
)

configure_plugin(${PLUGIN_NAME})
