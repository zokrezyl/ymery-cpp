# Unit tests using boost-ext/ut
# Tests run from build directory root where plugins/ is located

# Disable C++20 modules for boost.ut on MSVC (causes module errors)
if(MSVC)
    add_compile_definitions(BOOST_UT_DISABLE_MODULE)
endif()

# Embedded plugin sources needed for tests that use ymery_lib
set(EMBEDDED_PLUGIN_SOURCES
    ${CMAKE_SOURCE_DIR}/src/ymery/plugins/frontend/imgui/main.cpp
    ${CMAKE_SOURCE_DIR}/src/ymery/plugins/backend/data-tree.cpp
    ${CMAKE_SOURCE_DIR}/src/ymery/plugins/backend/simple-data-tree.cpp
)

# Kernel plugin tests
add_executable(kernel_test kernel_test.cpp ${EMBEDDED_PLUGIN_SOURCES})
target_link_libraries(kernel_test PRIVATE ymery_lib ut)
target_include_directories(kernel_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(kernel_test PRIVATE YMERY_EMBEDDED_PLUGINS=1)
add_test(NAME kernel_test COMMAND kernel_test WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

# Waveform plugin tests
add_executable(waveform_test waveform_test.cpp ${EMBEDDED_PLUGIN_SOURCES})
target_link_libraries(waveform_test PRIVATE ymery_lib ut)
target_include_directories(waveform_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(waveform_test PRIVATE YMERY_EMBEDDED_PLUGINS=1)
add_test(NAME waveform_test COMMAND waveform_test WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

# Plugin manager tests
add_executable(plugin_manager_test plugin_manager_test.cpp ${EMBEDDED_PLUGIN_SOURCES})
target_link_libraries(plugin_manager_test PRIVATE ymery_lib ut)
target_include_directories(plugin_manager_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(plugin_manager_test PRIVATE YMERY_EMBEDDED_PLUGINS=1)
add_test(NAME plugin_manager_test COMMAND plugin_manager_test WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

# foreach-child tests (DataBag navigation)
add_executable(foreach_child_test foreach_child_test.cpp ${EMBEDDED_PLUGIN_SOURCES})
target_link_libraries(foreach_child_test PRIVATE ymery_lib ut)
target_include_directories(foreach_child_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(foreach_child_test PRIVATE YMERY_EMBEDDED_PLUGINS=1)
add_test(NAME foreach_child_test COMMAND foreach_child_test WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
