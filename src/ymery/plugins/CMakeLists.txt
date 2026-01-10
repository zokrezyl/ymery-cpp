# Plugins subdirectories
add_subdirectory(frontend)
add_subdirectory(backend)

# Export all plugin targets to parent scope
set(YMERY_ALL_PLUGIN_TARGETS
    ${YMERY_FRONTEND_PLUGIN_TARGETS}
    ${YMERY_BACKEND_PLUGIN_TARGETS}
    PARENT_SCOPE
)

# Export embedded plugin objects to parent scope
set(YMERY_EMBEDDED_PLUGIN_OBJECTS ${YMERY_EMBEDDED_PLUGIN_OBJECTS} PARENT_SCOPE)
