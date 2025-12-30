# Build each tree-like backend as a shared library plugin

# On Android, output to the default location so Gradle bundles them in APK
if(YMERY_ANDROID)
    set(PLUGIN_OUTPUT_DIR "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
else()
    set(PLUGIN_OUTPUT_DIR "${CMAKE_BINARY_DIR}/plugins")
endif()

# For Emscripten: compile plugins as side modules for dynamic linking
if(YMERY_WEB)
    set(EMSCRIPTEN_SIDE_MODULE_FLAGS "-sSIDE_MODULE=1 -sEXPORTED_FUNCTIONS=['_name','_type','_create'] -sEXPORT_ALL=1")
endif()

# Helper macro to configure a plugin target (handles Emscripten side module flags)
macro(configure_plugin target)
    if(YMERY_WEB)
        set_target_properties(${target} PROPERTIES
            SUFFIX ".wasm"
            LINK_FLAGS "${EMSCRIPTEN_SIDE_MODULE_FLAGS}"
        )
    endif()
endmacro()

set(BACKEND_PLUGINS
    simple-data-tree
    data-tree
    kernel
)

foreach(plugin ${BACKEND_PLUGINS})
    add_library(${plugin} SHARED ${plugin}.cpp)
    target_link_libraries(${plugin} PRIVATE ymery_lib)
    target_include_directories(${plugin} PRIVATE ${CMAKE_SOURCE_DIR}/src)
    set_target_properties(${plugin} PROPERTIES
        PREFIX ""
        OUTPUT_NAME "${plugin}"
        LIBRARY_OUTPUT_DIRECTORY "${PLUGIN_OUTPUT_DIR}"
        LIBRARY_OUTPUT_DIRECTORY_DEBUG "${PLUGIN_OUTPUT_DIR}"
        LIBRARY_OUTPUT_DIRECTORY_RELEASE "${PLUGIN_OUTPUT_DIR}"
        LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO "${PLUGIN_OUTPUT_DIR}"
        LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL "${PLUGIN_OUTPUT_DIR}"
    )
    configure_plugin(${plugin})
endforeach()

# Waveform plugin (requires threading)
add_library(waveform SHARED waveform.cpp)
target_link_libraries(waveform PRIVATE ymery_lib)
target_include_directories(waveform PRIVATE ${CMAKE_SOURCE_DIR}/src)
set_target_properties(waveform PROPERTIES
    PREFIX ""
    OUTPUT_NAME "waveform"
    LIBRARY_OUTPUT_DIRECTORY "${PLUGIN_OUTPUT_DIR}"
    LIBRARY_OUTPUT_DIRECTORY_DEBUG "${PLUGIN_OUTPUT_DIR}"
    LIBRARY_OUTPUT_DIRECTORY_RELEASE "${PLUGIN_OUTPUT_DIR}"
    LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO "${PLUGIN_OUTPUT_DIR}"
    LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL "${PLUGIN_OUTPUT_DIR}"
)
configure_plugin(waveform)

# Platform-specific audio plugins (not available on web)
if(NOT YMERY_WEB)

# ALSA plugin (Linux only)
if(UNIX AND NOT APPLE AND NOT YMERY_ANDROID)
    find_package(ALSA)
    if(ALSA_FOUND)
        add_library(alsa-plugin SHARED alsa.cpp)
        target_link_libraries(alsa-plugin PRIVATE ymery_lib ${ALSA_LIBRARIES})
        target_include_directories(alsa-plugin PRIVATE ${CMAKE_SOURCE_DIR}/src ${ALSA_INCLUDE_DIRS})
        set_target_properties(alsa-plugin PROPERTIES
            PREFIX ""
            OUTPUT_NAME "alsa"
            LIBRARY_OUTPUT_DIRECTORY "${PLUGIN_OUTPUT_DIR}"
            LIBRARY_OUTPUT_DIRECTORY_DEBUG "${PLUGIN_OUTPUT_DIR}"
            LIBRARY_OUTPUT_DIRECTORY_RELEASE "${PLUGIN_OUTPUT_DIR}"
            LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO "${PLUGIN_OUTPUT_DIR}"
            LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL "${PLUGIN_OUTPUT_DIR}"
        )
        message(STATUS "ALSA plugin enabled")
    else()
        message(STATUS "ALSA not found - alsa plugin disabled")
    endif()
endif()

# JACK plugin (Linux and macOS)
if(UNIX AND NOT YMERY_ANDROID)
    find_library(JACK_LIBRARY jack)
    find_path(JACK_INCLUDE_DIR jack/jack.h)
    if(JACK_LIBRARY AND JACK_INCLUDE_DIR)
        add_library(jack-plugin SHARED jack.cpp)
        target_link_libraries(jack-plugin PRIVATE ymery_lib ${JACK_LIBRARY})
        target_include_directories(jack-plugin PRIVATE ${CMAKE_SOURCE_DIR}/src ${JACK_INCLUDE_DIR})
        set_target_properties(jack-plugin PROPERTIES
            PREFIX ""
            OUTPUT_NAME "jack"
            LIBRARY_OUTPUT_DIRECTORY "${PLUGIN_OUTPUT_DIR}"
            LIBRARY_OUTPUT_DIRECTORY_DEBUG "${PLUGIN_OUTPUT_DIR}"
            LIBRARY_OUTPUT_DIRECTORY_RELEASE "${PLUGIN_OUTPUT_DIR}"
            LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO "${PLUGIN_OUTPUT_DIR}"
            LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL "${PLUGIN_OUTPUT_DIR}"
        )
        message(STATUS "JACK plugin enabled")
    else()
        message(STATUS "JACK not found - jack plugin disabled")
    endif()
endif()

# PipeWire plugin (Linux only)
if(UNIX AND NOT APPLE AND NOT YMERY_ANDROID)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(PIPEWIRE libpipewire-0.3)
    endif()
    if(PIPEWIRE_FOUND)
        add_library(pipewire-plugin SHARED pipewire.cpp)
        target_link_libraries(pipewire-plugin PRIVATE ymery_lib ${PIPEWIRE_LIBRARIES})
        target_include_directories(pipewire-plugin PRIVATE ${CMAKE_SOURCE_DIR}/src ${PIPEWIRE_INCLUDE_DIRS})
        target_compile_options(pipewire-plugin PRIVATE ${PIPEWIRE_CFLAGS_OTHER})
        set_target_properties(pipewire-plugin PROPERTIES
            PREFIX ""
            OUTPUT_NAME "pipewire"
            LIBRARY_OUTPUT_DIRECTORY "${PLUGIN_OUTPUT_DIR}"
            LIBRARY_OUTPUT_DIRECTORY_DEBUG "${PLUGIN_OUTPUT_DIR}"
            LIBRARY_OUTPUT_DIRECTORY_RELEASE "${PLUGIN_OUTPUT_DIR}"
            LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO "${PLUGIN_OUTPUT_DIR}"
            LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL "${PLUGIN_OUTPUT_DIR}"
        )
        message(STATUS "PipeWire plugin enabled")
    else()
        message(STATUS "PipeWire not found - pipewire plugin disabled")
    endif()
endif()

# Core Audio plugin (macOS only)
if(APPLE)
    find_library(COREAUDIO_LIBRARY CoreAudio)
    find_library(AUDIOTOOLBOX_LIBRARY AudioToolbox)
    find_library(COREFOUNDATION_LIBRARY CoreFoundation)
    if(COREAUDIO_LIBRARY AND AUDIOTOOLBOX_LIBRARY)
        add_library(coreaudio-plugin SHARED coreaudio.cpp)
        target_link_libraries(coreaudio-plugin PRIVATE
            ymery_lib
            ${COREAUDIO_LIBRARY}
            ${AUDIOTOOLBOX_LIBRARY}
            ${COREFOUNDATION_LIBRARY}
        )
        target_include_directories(coreaudio-plugin PRIVATE ${CMAKE_SOURCE_DIR}/src)
        set_target_properties(coreaudio-plugin PROPERTIES
            PREFIX ""
            OUTPUT_NAME "coreaudio"
            LIBRARY_OUTPUT_DIRECTORY "${PLUGIN_OUTPUT_DIR}"
            LIBRARY_OUTPUT_DIRECTORY_DEBUG "${PLUGIN_OUTPUT_DIR}"
            LIBRARY_OUTPUT_DIRECTORY_RELEASE "${PLUGIN_OUTPUT_DIR}"
            LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO "${PLUGIN_OUTPUT_DIR}"
            LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL "${PLUGIN_OUTPUT_DIR}"
        )
        message(STATUS "Core Audio plugin enabled")
    else()
        message(STATUS "Core Audio frameworks not found - coreaudio plugin disabled")
    endif()
endif()

endif() # NOT YMERY_WEB

# Filesystem plugin (exposes both tree-like and device-manager)
add_subdirectory(filesystem)
