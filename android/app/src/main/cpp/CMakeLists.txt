cmake_minimum_required(VERSION 3.22.1)
project(ymery_android)

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

# Android app native library
add_library(ymery_android SHARED
    main.cpp
)

target_include_directories(ymery_android PRIVATE
    ${ANDROID_NDK}/sources/android/native_app_glue
)

target_link_libraries(ymery_android
    ymery
    native_app_glue
    android
    log
    EGL
    GLESv3
)
