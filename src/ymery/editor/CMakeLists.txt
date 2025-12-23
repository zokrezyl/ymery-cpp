# ymery-editor - Widget Layout Editor

set(EDITOR_SOURCES
    main.cpp
    editor_app.cpp
    widget_tree.cpp
    layout_model.cpp
    editor_canvas.cpp
)

add_executable(ymery-editor ${EDITOR_SOURCES})

target_include_directories(ymery-editor PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/third_party/imgui
    ${CMAKE_SOURCE_DIR}/third_party/imgui/backends
)

target_link_libraries(ymery-editor PRIVATE
    ymery
    imgui
    glfw
    spdlog::spdlog
)

if(YMERY_USE_WEBGPU)
    target_link_libraries(ymery-editor PRIVATE webgpu)
    target_compile_definitions(ymery-editor PRIVATE YMERY_USE_WEBGPU)
else()
    find_package(OpenGL REQUIRED)
    target_link_libraries(ymery-editor PRIVATE OpenGL::GL)
endif()
