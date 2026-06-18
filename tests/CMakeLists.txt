file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*_test.cc)

foreach(test_source IN LISTS TEST_SOURCES)
    get_filename_component(test_name ${test_source} NAME_WE)
    add_executable(${test_name} ${test_source})
    target_link_libraries(${test_name}
        PRIVATE
        error_system::error_system
        GTest::gtest_main
    )
    error_system_apply_warnings_only(${test_name})

    if (MSVC)
        target_compile_options(${test_name} PRIVATE /Zi)
        target_link_options(${test_name} PRIVATE /DEBUG)
    endif()

    gtest_discover_tests(${test_name} DISCOVERY_TIMEOUT 30)
endforeach()

if(ERROR_SYSTEM_BUILD_PERF_TESTS)
    file(GLOB PERF_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/perf/*_benchmark.cc)

    foreach(perf_source IN LISTS PERF_SOURCES)
        get_filename_component(perf_name ${perf_source} NAME_WE)
        add_executable(${perf_name} ${perf_source})
        target_link_libraries(${perf_name} PRIVATE error_system)
        error_system_apply_warnings_only(${perf_name})
        set_target_properties(${perf_name} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/perf)
    endforeach()
endif()
