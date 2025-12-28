cmake_minimum_required(VERSION 3.22.1)
project(ymery_android)

# Include the main ymery project
set(YMERY_ANDROID ON CACHE BOOL "" FORCE)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../../../../.. ${CMAKE_CURRENT_BINARY_DIR}/ymery)

# Add native_app_glue from NDK
add_library(native_app_glue STATIC
    ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c
)
target_include_directories(native_app_glue PUBLIC
    ${ANDROID_NDK}/sources/android/native_app_glue
)

# Android app native library
add_library(ymery_android SHARED
    main.cpp
)

target_include_directories(ymery_android PRIVATE
    ${ANDROID_NDK}/sources/android/native_app_glue
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../src
)

# Base libraries
set(YMERY_ANDROID_LIBS
    ymery_lib
    -Wl,--whole-archive
    native_app_glue
    -Wl,--no-whole-archive
    android
    log
)

# Add backend-specific libraries
if(YMERY_USE_WEBGPU)
    # WebGPU backend - link pre-built wgpu-native library
    set(WGPU_LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../libs/${ANDROID_ABI})
    add_library(wgpu_native SHARED IMPORTED)
    set_target_properties(wgpu_native PROPERTIES
        IMPORTED_LOCATION ${WGPU_LIB_DIR}/libwgpu_native.so
    )
    # Add wgpu-native include path
    target_include_directories(ymery_android PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}/ymery/_deps/webgpu-src/include
    )
    list(APPEND YMERY_ANDROID_LIBS wgpu_native)
else()
    # OpenGL backend - link EGL and GLESv3
    list(APPEND YMERY_ANDROID_LIBS EGL GLESv3)
endif()

target_link_libraries(ymery_android ${YMERY_ANDROID_LIBS})
