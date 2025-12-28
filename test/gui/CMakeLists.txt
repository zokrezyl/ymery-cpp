# GUI tests using imgui_test_engine

# Fetch imgui_test_engine via CPM (version must match imgui version)
# imgui 1.91.6 requires imgui_test_engine v1.91.6
CPMAddPackage(
    NAME imgui_test_engine
    GITHUB_REPOSITORY ocornut/imgui_test_engine
    GIT_TAG v1.91.6
    DOWNLOAD_ONLY YES
)

if(NOT imgui_test_engine_ADDED)
    message(FATAL_ERROR "Failed to fetch imgui_test_engine")
endif()

set(IMGUI_TEST_ENGINE_DIR ${imgui_test_engine_SOURCE_DIR}/imgui_test_engine)

# imgui_test_engine library
add_library(imgui_test_engine STATIC
    ${IMGUI_TEST_ENGINE_DIR}/imgui_te_engine.cpp
    ${IMGUI_TEST_ENGINE_DIR}/imgui_te_context.cpp
    ${IMGUI_TEST_ENGINE_DIR}/imgui_te_coroutine.cpp
    ${IMGUI_TEST_ENGINE_DIR}/imgui_te_exporters.cpp
    ${IMGUI_TEST_ENGINE_DIR}/imgui_te_perftool.cpp
    ${IMGUI_TEST_ENGINE_DIR}/imgui_te_ui.cpp
    ${IMGUI_TEST_ENGINE_DIR}/imgui_te_utils.cpp
    ${IMGUI_TEST_ENGINE_DIR}/imgui_capture_tool.cpp
)

# IMPORTANT: Link imgui FIRST so its headers are found before test engine's bundled copy
target_link_libraries(imgui_test_engine PUBLIC
    imgui
    implot
)

# Add test engine includes AFTER imgui (so imgui headers take precedence)
target_include_directories(imgui_test_engine PUBLIC
    ${IMGUI_TEST_ENGINE_DIR}
    ${imgui_test_engine_SOURCE_DIR}
)

# Enable test engine support in imgui and test engine itself
target_compile_definitions(imgui_test_engine PUBLIC
    IMGUI_TEST_ENGINE_ENABLE_STD_FUNCTION=1
    IMGUI_TEST_ENGINE_ENABLE_COROUTINE_STDTHREAD_IMPL=1
    IMGUI_ENABLE_TEST_ENGINE=1
)

# Also need to rebuild imgui with test engine support
# Add the define to imgui target
target_compile_definitions(imgui PUBLIC IMGUI_ENABLE_TEST_ENGINE=1)

# IMPORTANT: ymery_lib links to imgui, which now has test engine hooks.
# ymery_lib must also link to imgui_test_engine to resolve those symbols.
target_link_libraries(ymery_lib PUBLIC imgui_test_engine)

# Test layouts directory
set(TEST_LAYOUTS_DIR ${CMAKE_SOURCE_DIR}/test/layouts)

# GUI test executable
add_executable(gui_test
    gui_test_main.cpp
    tree_node_tests.cpp
    basic_widget_tests.cpp
    filesystem_tests.cpp
    ${CMAKE_SOURCE_DIR}/src/ymery/plugins/backend/filesystem/common.cpp
)

target_link_libraries(gui_test PRIVATE
    ymery_lib
    imgui_test_engine
    spdlog::spdlog
)

target_include_directories(gui_test PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_compile_definitions(gui_test PRIVATE
    TEST_LAYOUTS_DIR="${TEST_LAYOUTS_DIR}"
)

# Register as a test
add_test(NAME gui_test COMMAND gui_test)
