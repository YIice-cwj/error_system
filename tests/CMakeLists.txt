file(GLOB_RECURSE TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/*_test.cc)

foreach(test_source IN LISTS TEST_SOURCES)

    get_filename_component(test_name ${test_source} NAME_WE)

    add_executable(${test_name} ${test_source})

    target_link_libraries(${test_name}
        PRIVATE
        error_system
        GTest::gtest_main
    )

    if (MSVC)
        target_compile_options(${test_name} PRIVATE /Zi)
        target_link_options(${test_name} PRIVATE /DEBUG)
    endif()

    gtest_discover_tests(${test_name} DISCOVERY_TIMEOUT 30)
endforeach()
