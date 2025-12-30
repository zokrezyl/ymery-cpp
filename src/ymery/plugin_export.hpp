#pragma once
// Plugin export macros for static linking on web
//
// For web builds, plugins are compiled with unique symbol names using
// compile definitions: YMERY_PLUGIN_NAME_PREFIX=foo_plugin_
//
// Plugins should use PLUGIN_EXPORT_NAME, PLUGIN_EXPORT_TYPE, PLUGIN_EXPORT_CREATE
// to define their export functions.

// If a plugin name prefix is defined, use it
#ifdef YMERY_PLUGIN_NAME_PREFIX
    #define PLUGIN_EXPORT_PASTE2(a, b) a ## b
    #define PLUGIN_EXPORT_PASTE(a, b) PLUGIN_EXPORT_PASTE2(a, b)
    #define PLUGIN_EXPORT_NAME PLUGIN_EXPORT_PASTE(YMERY_PLUGIN_NAME_PREFIX, name)
    #define PLUGIN_EXPORT_TYPE PLUGIN_EXPORT_PASTE(YMERY_PLUGIN_NAME_PREFIX, type)
    #define PLUGIN_EXPORT_CREATE PLUGIN_EXPORT_PASTE(YMERY_PLUGIN_NAME_PREFIX, create)
#else
    // Default: use plain name, type, create for dynamic linking
    #define PLUGIN_EXPORT_NAME name
    #define PLUGIN_EXPORT_TYPE type
    #define PLUGIN_EXPORT_CREATE create
#endif
