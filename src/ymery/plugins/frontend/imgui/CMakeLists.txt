# imgui plugin - core ImGui widgets
# This plugin provides: button, text, checkbox, window, etc.
# Linked directly into main executable (not a dynamic plugin)

add_library(imgui_plugin_embedded OBJECT main.cpp)
target_link_libraries(imgui_plugin_embedded PRIVATE ymery_lib)
target_include_directories(imgui_plugin_embedded PRIVATE ${CMAKE_SOURCE_DIR}/src)

# Export to parent scope for linking into ymery
set(YMERY_EMBEDDED_PLUGIN_OBJECTS $<TARGET_OBJECTS:imgui_plugin_embedded> PARENT_SCOPE)
