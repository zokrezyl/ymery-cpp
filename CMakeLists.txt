cmake_minimum_required(VERSION 3.20)
project(ymery VERSION 0.1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Windows: Export all symbols from shared libraries to generate import libraries (.lib)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

# Set output directories to avoid multi-config subdirectories (Release/Debug)
# This ensures plugins can find the ymery library
# For single-config generators (Make, Ninja)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
# For multi-config generators (Visual Studio, Xcode) - set all common configs
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_BINARY_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_MINSIZEREL ${CMAKE_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL ${CMAKE_BINARY_DIR})

# Detect Emscripten
if(EMSCRIPTEN)
    message(STATUS "Building for Web (Emscripten)")
    set(YMERY_WEB ON)
    set(YMERY_USE_WEBGPU ON)  # Web always uses WebGPU
else()
    set(YMERY_WEB OFF)
endif()

# Android build option
option(YMERY_ANDROID "Build for Android" OFF)
if(YMERY_ANDROID)
    message(STATUS "Building for Android")
endif()

# Graphics backend option (ignored for Emscripten - always WebGPU)
option(YMERY_USE_WEBGPU "Use WebGPU backend instead of OpenGL" OFF)
if(YMERY_WEB)
    set(YMERY_USE_WEBGPU ON CACHE BOOL "" FORCE)
endif()

message(STATUS "Graphics backend: ${YMERY_USE_WEBGPU}")
if(YMERY_USE_WEBGPU)
    message(STATUS "Building with WebGPU backend")
else()
    message(STATUS "Building with OpenGL backend")
endif()

# CPM.cmake for dependency management
set(CPM_DOWNLOAD_VERSION 0.40.2)
if(CPM_SOURCE_CACHE)
    set(CPM_DOWNLOAD_LOCATION "${CPM_SOURCE_CACHE}/cpm/CPM_${CPM_DOWNLOAD_VERSION}.cmake")
elseif(DEFINED ENV{CPM_SOURCE_CACHE})
    set(CPM_DOWNLOAD_LOCATION "$ENV{CPM_SOURCE_CACHE}/cpm/CPM_${CPM_DOWNLOAD_VERSION}.cmake")
else()
    set(CPM_DOWNLOAD_LOCATION "${CMAKE_BINARY_DIR}/cmake/CPM_${CPM_DOWNLOAD_VERSION}.cmake")
endif()
if(NOT (EXISTS ${CPM_DOWNLOAD_LOCATION}))
    message(STATUS "Downloading CPM.cmake to ${CPM_DOWNLOAD_LOCATION}")
    file(DOWNLOAD
        https://github.com/cpm-cmake/CPM.cmake/releases/download/v${CPM_DOWNLOAD_VERSION}/CPM.cmake
        ${CPM_DOWNLOAD_LOCATION}
    )
endif()
include(${CPM_DOWNLOAD_LOCATION})

# Dependencies
CPMAddPackage(
    NAME spdlog
    GITHUB_REPOSITORY gabime/spdlog
    VERSION 1.14.1
)

CPMAddPackage(
    NAME yaml-cpp
    GITHUB_REPOSITORY jbeder/yaml-cpp
    GIT_TAG 0.8.0
    OPTIONS "YAML_CPP_BUILD_TESTS OFF" "YAML_CPP_BUILD_TOOLS OFF"
)

# tl::expected - better std::expected compatibility for Android NDK
if(YMERY_ANDROID)
    CPMAddPackage(
        NAME tl_expected
        GITHUB_REPOSITORY TartanLlama/expected
        GIT_TAG v1.1.0
        OPTIONS "EXPECTED_BUILD_TESTS OFF"
    )
endif()

# GLFW - Emscripten and Android provide their own windowing
if(NOT YMERY_WEB AND NOT YMERY_ANDROID)
    CPMAddPackage(
        NAME glfw
        GITHUB_REPOSITORY glfw/glfw
        GIT_TAG 3.4
        OPTIONS
            "GLFW_BUILD_EXAMPLES OFF"
            "GLFW_BUILD_TESTS OFF"
            "GLFW_BUILD_DOCS OFF"
            "GLFW_BUILD_X11 ON"
            "GLFW_BUILD_WAYLAND OFF"
    )
endif()

CPMAddPackage(
    NAME imgui
    GITHUB_REPOSITORY ocornut/imgui
    VERSION 1.91.6-docking
    DOWNLOAD_ONLY YES
)

# WebGPU native library (wgpu-native v0.19 - compatible with ImGui)
# Not needed for Emscripten - browser provides WebGPU
if(YMERY_USE_WEBGPU AND NOT YMERY_WEB)
    CPMAddPackage(
        NAME webgpu
        GITHUB_REPOSITORY eliemichel/WebGPU-distribution
        GIT_TAG wgpu-v0.19.4.1
    )
endif()

if(imgui_ADDED)
    # Core ImGui files
    set(IMGUI_SOURCES
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_demo.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    )

    # Platform backend
    if(YMERY_ANDROID)
        list(APPEND IMGUI_SOURCES ${imgui_SOURCE_DIR}/backends/imgui_impl_android.cpp)
    else()
        list(APPEND IMGUI_SOURCES ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp)
    endif()

    # Renderer backend
    if(YMERY_USE_WEBGPU)
        list(APPEND IMGUI_SOURCES ${imgui_SOURCE_DIR}/backends/imgui_impl_wgpu.cpp)
    else()
        list(APPEND IMGUI_SOURCES ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp)
    endif()

    add_library(imgui STATIC ${IMGUI_SOURCES})
    target_include_directories(imgui PUBLIC
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
    )

    if(YMERY_WEB)
        # Emscripten - use USE_GLFW and USE_WEBGPU
        target_compile_definitions(imgui PUBLIC YMERY_USE_WEBGPU=1 YMERY_WEB=1)
    elseif(YMERY_ANDROID)
        # Android - OpenGL ES
        target_compile_definitions(imgui PUBLIC YMERY_ANDROID=1)
        target_link_libraries(imgui PUBLIC android log EGL GLESv3)
    elseif(YMERY_USE_WEBGPU)
        # Native WebGPU (wgpu-native)
        target_link_libraries(imgui PUBLIC glfw webgpu)
        target_compile_definitions(imgui PUBLIC
            YMERY_USE_WEBGPU=1
            IMGUI_IMPL_WEBGPU_BACKEND_WGPU=1
        )
    else()
        # OpenGL on X11
        target_link_libraries(imgui PUBLIC glfw)
        find_package(OpenGL REQUIRED)
        target_link_libraries(imgui PUBLIC OpenGL::GL)
    endif()
endif()

# ImPlot - plotting library
CPMAddPackage(
    NAME implot
    GITHUB_REPOSITORY epezent/implot
    VERSION 0.16
    DOWNLOAD_ONLY YES
)

if(implot_ADDED)
    add_library(implot STATIC
        ${implot_SOURCE_DIR}/implot.cpp
        ${implot_SOURCE_DIR}/implot_items.cpp
    )
    target_include_directories(implot PUBLIC ${implot_SOURCE_DIR})
    target_link_libraries(implot PUBLIC imgui)
endif()

# imgui-node-editor - DISABLED due to compatibility issues with ImGui 1.90+
# CPMAddPackage(
#     NAME imgui_node_editor
#     GITHUB_REPOSITORY thedmd/imgui-node-editor
#     VERSION 0.9.3
#     DOWNLOAD_ONLY YES
# )

# imgui_markdown - markdown renderer
CPMAddPackage(
    NAME imgui_markdown
    GITHUB_REPOSITORY juliettef/imgui_markdown
    GIT_TAG main
    DOWNLOAD_ONLY YES
)

if(imgui_markdown_ADDED)
    # imgui_markdown is header-only
    add_library(imgui_md INTERFACE)
    target_include_directories(imgui_md INTERFACE ${imgui_markdown_SOURCE_DIR})
endif()

# imgui-knobs - Knob widgets
CPMAddPackage(
    NAME imgui_knobs
    GITHUB_REPOSITORY altschuler/imgui-knobs
    GIT_TAG main
    DOWNLOAD_ONLY YES
)

if(imgui_knobs_ADDED)
    add_library(imgui_knobs STATIC ${imgui_knobs_SOURCE_DIR}/imgui-knobs.cpp)
    target_include_directories(imgui_knobs PUBLIC ${imgui_knobs_SOURCE_DIR})
    target_link_libraries(imgui_knobs PUBLIC imgui)
endif()

# imspinner - Spinner/loading widgets
CPMAddPackage(
    NAME imspinner
    GITHUB_REPOSITORY dalerank/imspinner
    GIT_TAG master
    DOWNLOAD_ONLY YES
)

if(imspinner_ADDED)
    # imspinner is header-only
    add_library(imspinner INTERFACE)
    target_include_directories(imspinner INTERFACE ${imspinner_SOURCE_DIR})
endif()

# imgui-toggle - Toggle switch widgets
CPMAddPackage(
    NAME imgui_toggle
    GITHUB_REPOSITORY cmdwtf/imgui_toggle
    GIT_TAG main
    DOWNLOAD_ONLY YES
)

if(imgui_toggle_ADDED)
    add_library(imgui_toggle STATIC
        ${imgui_toggle_SOURCE_DIR}/imgui_toggle.cpp
        ${imgui_toggle_SOURCE_DIR}/imgui_toggle_palette.cpp
        ${imgui_toggle_SOURCE_DIR}/imgui_toggle_presets.cpp
        ${imgui_toggle_SOURCE_DIR}/imgui_toggle_renderer.cpp
    )
    target_include_directories(imgui_toggle PUBLIC ${imgui_toggle_SOURCE_DIR})
    target_link_libraries(imgui_toggle PUBLIC imgui)
endif()

# imcoolbar - macOS-style dock bar
CPMAddPackage(
    NAME imcoolbar
    GITHUB_REPOSITORY aiekick/ImCoolBar
    GIT_TAG master
    DOWNLOAD_ONLY YES
)

if(imcoolbar_ADDED)
    add_library(imcoolbar STATIC ${imcoolbar_SOURCE_DIR}/ImCoolBar.cpp)
    target_include_directories(imcoolbar PUBLIC ${imcoolbar_SOURCE_DIR})
    target_link_libraries(imcoolbar PUBLIC imgui)
endif()

# imguizmo - 3D gizmo manipulation
CPMAddPackage(
    NAME imguizmo
    GITHUB_REPOSITORY CedricGuillemet/ImGuizmo
    GIT_TAG master
    DOWNLOAD_ONLY YES
)

if(imguizmo_ADDED)
    add_library(imguizmo STATIC
        ${imguizmo_SOURCE_DIR}/ImGuizmo.cpp
        ${imguizmo_SOURCE_DIR}/ImGradient.cpp
        ${imguizmo_SOURCE_DIR}/ImCurveEdit.cpp
        ${imguizmo_SOURCE_DIR}/ImSequencer.cpp
        ${imguizmo_SOURCE_DIR}/GraphEditor.cpp
    )
    target_include_directories(imguizmo PUBLIC ${imguizmo_SOURCE_DIR})
    target_link_libraries(imguizmo PUBLIC imgui)
endif()

# implot3d - 3D plotting
CPMAddPackage(
    NAME implot3d
    GITHUB_REPOSITORY brenocq/implot3d
    GIT_TAG main
    DOWNLOAD_ONLY YES
)

if(implot3d_ADDED)
    add_library(implot3d STATIC
        ${implot3d_SOURCE_DIR}/implot3d.cpp
        ${implot3d_SOURCE_DIR}/implot3d_items.cpp
    )
    target_include_directories(implot3d PUBLIC ${implot3d_SOURCE_DIR})
    target_link_libraries(implot3d PUBLIC imgui)
endif()

# nanovg - Vector graphics (requires OpenGL, desktop only)
if(NOT YMERY_WEB AND NOT YMERY_ANDROID)
    CPMAddPackage(
        NAME nanovg
        GITHUB_REPOSITORY memononen/nanovg
        GIT_TAG master
        DOWNLOAD_ONLY YES
    )

    if(nanovg_ADDED)
        add_library(nanovg STATIC ${nanovg_SOURCE_DIR}/src/nanovg.c)
        target_include_directories(nanovg PUBLIC ${nanovg_SOURCE_DIR}/src)
        if(NOT YMERY_USE_WEBGPU)
            target_compile_definitions(nanovg PUBLIC NANOVG_GL2_IMPLEMENTATION)
        endif()
    endif()
endif()

# immvision - Image visualization (requires OpenCV)
# CPMAddPackage(
#     NAME immvision
#     GITHUB_REPOSITORY pthom/immvision
#     GIT_TAG main
#     DOWNLOAD_ONLY YES
# )
# Note: immvision requires OpenCV, enable if OpenCV is available

# Core library sources (shared between all platforms)
set(YMERY_CORE_SOURCES
    src/ymery/types.cpp
    src/ymery/data_bag.cpp
    src/ymery/dispatcher.cpp
    src/ymery/lang.cpp
    src/ymery/plugin_manager.cpp
    src/ymery/frontend/widget.cpp
    src/ymery/frontend/widget_factory.cpp
    src/ymery/frontend/composite.cpp
    src/ymery/backend/audio_buffer.cpp
)

# App.cpp has backend support for GLFW (desktop), WebGPU, and Android (EGL)
list(APPEND YMERY_CORE_SOURCES src/ymery/app.cpp)

# Core library
if(YMERY_WEB OR YMERY_ANDROID)
    # Static library for Emscripten and Android (no shared libs / dynamic loading)
    add_library(ymery STATIC ${YMERY_CORE_SOURCES})
else()
    add_library(ymery SHARED ${YMERY_CORE_SOURCES})
endif()

target_include_directories(ymery PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

if(YMERY_WEB)
    target_link_libraries(ymery PUBLIC
        yaml-cpp
        imgui
        implot
        imgui_md
        imgui_knobs
        imspinner
        imgui_toggle
        imcoolbar
        imguizmo
        implot3d
        spdlog::spdlog
    )
    target_compile_definitions(ymery PUBLIC YMERY_WEB=1)
elseif(YMERY_ANDROID)
    target_link_libraries(ymery PUBLIC
        yaml-cpp
        imgui
        implot
        imgui_md
        imgui_knobs
        imspinner
        imgui_toggle
        imcoolbar
        imguizmo
        implot3d
        spdlog::spdlog
        android
        log
        EGL
        GLESv3
    )
    # Add tl::expected include directory (header-only library)
    target_include_directories(ymery PUBLIC ${tl_expected_SOURCE_DIR}/include)
    # Add NDK native_app_glue include directory
    target_include_directories(ymery PUBLIC ${ANDROID_NDK}/sources/android/native_app_glue)
    target_compile_definitions(ymery PUBLIC YMERY_ANDROID=1)
else()
    target_link_libraries(ymery PUBLIC
        yaml-cpp
        imgui
        implot
        imgui_md
        imgui_knobs
        imspinner
        imgui_toggle
        imcoolbar
        imguizmo
        implot3d
        nanovg
        glfw
        spdlog::spdlog
    )
    # dl library is only needed on Unix-like systems
    if(UNIX)
        target_link_libraries(ymery PUBLIC dl)
    endif()
endif()

# Main executable (not for Android - needs native activity)
if(NOT YMERY_ANDROID)
    add_executable(ymery-cli src/main.cpp)
    target_link_libraries(ymery-cli PRIVATE ymery)

    if(YMERY_WEB)
        # Emscripten linker flags
        set_target_properties(ymery-cli PROPERTIES
            SUFFIX ".html"
            LINK_FLAGS "-s USE_GLFW=3 -s USE_WEBGPU=1 -s WASM=1 -s ALLOW_MEMORY_GROWTH=1 -s NO_EXIT_RUNTIME=0 -s ASSERTIONS=1 --preload-file ${CMAKE_SOURCE_DIR}/layout@/layout"
        )
    else()
        # Native: enable symbol exports for plugins
        set_target_properties(ymery-cli PROPERTIES ENABLE_EXPORTS ON)
    endif()
endif()

# Widget Layout Editor (desktop only)
if(NOT YMERY_WEB AND NOT YMERY_ANDROID)
    add_executable(ymery-editor
        src/ymery/editor/main.cpp
        src/ymery/editor/editor_app.cpp
        src/ymery/editor/widget_tree.cpp
        src/ymery/editor/layout_model.cpp
        src/ymery/editor/editor_canvas.cpp
    )
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
endif()

# Plugins (desktop and Android - no dynamic loading on Web)
if(NOT YMERY_WEB)
    add_subdirectory(src/ymery/plugins)
endif()

# Examples (not for Android)
if(NOT YMERY_ANDROID)
    add_subdirectory(examples EXCLUDE_FROM_ALL)
endif()
