file(GLOB_RECURSE DEMO_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cc)

foreach(demo_source IN LISTS DEMO_SOURCES)
    get_filename_component(demo_name ${demo_source} NAME_WE)
    add_executable(${demo_name} ${demo_source})
    target_link_libraries(${demo_name}
        PRIVATE
        error_system
    )
    error_system_apply_warnings_only(${demo_name})

endforeach()
