cmake_minimum_required(VERSION 3.22.1)
project(ymery_editor_android)

# Include the main ymery project
set(YMERY_ANDROID ON CACHE BOOL "" FORCE)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../../../.. ${CMAKE_CURRENT_BINARY_DIR}/ymery)

# Add native_app_glue from NDK
add_library(native_app_glue STATIC
    ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c
)
target_include_directories(native_app_glue PUBLIC
    ${ANDROID_NDK}/sources/android/native_app_glue
)

# Editor source files (from main project)
set(EDITOR_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../src/ymery/editor/editor_app.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../src/ymery/editor/widget_tree.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../src/ymery/editor/layout_model.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../src/ymery/editor/editor_canvas.cpp
)

# Android editor native library
add_library(ymery_editor_android SHARED
    main.cpp
    ${EDITOR_SOURCES}
)

target_include_directories(ymery_editor_android PRIVATE
    ${ANDROID_NDK}/sources/android/native_app_glue
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../src
)

target_link_libraries(ymery_editor_android
    ymery
    -Wl,--whole-archive
    native_app_glue
    -Wl,--no-whole-archive
    android
    log
    EGL
    GLESv3
)
