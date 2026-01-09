# hex-editor plugin
set(PLUGIN_NAME hex-editor)

add_library(${PLUGIN_NAME} SHARED main.cpp)
target_link_libraries(${PLUGIN_NAME} PRIVATE ymery_lib)
target_include_directories(${PLUGIN_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/src)
set_target_properties(${PLUGIN_NAME} PROPERTIES PREFIX "" OUTPUT_NAME "${PLUGIN_NAME}")
set_plugin_output_dirs(${PLUGIN_NAME})
