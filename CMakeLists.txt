cmake_minimum_required(VERSION 3.15)

set(PROJECT_NAME "error_system")

set(VERSION_MAJOR 2)
set(VERSION_MINOR 3)
set(VERSION_PATCH 0)
set(VERSION_STRING ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH})

set(DESCRIPTION_STRING "error_system_v${VERSION_STRING}: c++17 错误码系统，提供错误码的定义和使用")

project(${PROJECT_NAME} VERSION ${VERSION_STRING} DESCRIPTION ${DESCRIPTION_STRING} LANGUAGES CXX)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo" "MinSizeRel")
endif()

set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(ERROR_SYSTEM_ENABLE_STACKTRACE "是否编译堆栈追踪功能 (关闭可减小体积，免链接 OS 底层库)" ON)
option(ERROR_SYSTEM_ENABLE_VALIDATION "是否编译严格的错误码验证器" ON)
option(ERROR_SYSTEM_ENABLE_LOCATION "是否编译源位置宏" ON)
option(ERROR_SYSTEM_ENABLE_LTO "启用 LTO/IPO" OFF)
option(ERROR_SYSTEM_ENABLE_PGO_GENERATE "启用 PGO 生成阶段" OFF)
option(ERROR_SYSTEM_ENABLE_PGO_USE "启用 PGO 使用阶段" OFF)
option(ERROR_SYSTEM_ENABLE_ASAN "启用地址安全检查器" OFF)
option(ERROR_SYSTEM_ENABLE_UBSAN "启用未定义行为检测器" OFF)
option(ERROR_SYSTEM_WARNINGS_AS_ERRORS "将警告视为错误" ON)
option(ERROR_SYSTEM_BUILD_TESTS "Build tests for error_system" ON)
option(ERROR_SYSTEM_BUILD_EXAMPLES "Build examples for error_system" ON)
option(ERROR_SYSTEM_BUILD_PERF_TESTS "Build performance tests" ON)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/error_system_performance.cmake)
find_package(Python3 COMPONENTS Interpreter QUIET)

set(ERROR_GENERATED_DIR "${CMAKE_BINARY_DIR}/generated_errors/include")

if(Python3_FOUND)
    set(ERROR_JSON_DIR "${CMAKE_SOURCE_DIR}/config/errors")
    set(ERROR_DOCS_OUT_DIR "${CMAKE_BINARY_DIR}/generated_errors")

    file(GLOB ERROR_JSON_FILES CONFIGURE_DEPENDS "${ERROR_JSON_DIR}/*.json")
    set(ALL_GENERATED_HEADERS "")

    foreach(JSON_FILE ${ERROR_JSON_FILES})
        get_filename_component(BASENAME ${JSON_FILE} NAME_WE)
        set(GENERATED_HEADER "${ERROR_GENERATED_DIR}/${BASENAME}.h")

        add_custom_command(
            OUTPUT ${GENERATED_HEADER}
            COMMAND Python3::Interpreter ${CMAKE_SOURCE_DIR}/script/script_py/generate_error_codes.py ${JSON_FILE} ${ERROR_GENERATED_DIR}
            DEPENDS ${JSON_FILE} ${CMAKE_SOURCE_DIR}/script/script_py/generate_error_codes.py
            COMMENT "🔨 [Code Generation] 正在从 ${BASENAME}.json 生成 C++ 错误码头文件..."
        )
        list(APPEND ALL_GENERATED_HEADERS ${GENERATED_HEADER})
    endforeach()

    set(DICT_HEADER "${ERROR_GENERATED_DIR}/error_dict.h")
    add_custom_command(
        OUTPUT ${DICT_HEADER}
        COMMAND Python3::Interpreter ${CMAKE_SOURCE_DIR}/script/script_py/generate_error_dict.py ${ERROR_JSON_DIR} ${DICT_HEADER}
        DEPENDS ${ERROR_JSON_FILES} ${CMAKE_SOURCE_DIR}/script/script_py/generate_error_dict.py
        COMMENT "🔨 [Code Generation] 正在生成 O(1) 极速错误字典..."
    )

    set(DOCS_MD_OUT "${ERROR_DOCS_OUT_DIR}/error_dictionary.md")
    add_custom_command(
        OUTPUT ${DOCS_MD_OUT}
        COMMAND Python3::Interpreter ${CMAKE_SOURCE_DIR}/script/script_py/generate_error_docs.py ${ERROR_JSON_DIR} ${DOCS_MD_OUT}
        DEPENDS ${ERROR_JSON_FILES} ${CMAKE_SOURCE_DIR}/script/script_py/generate_error_docs.py
        COMMENT "📝 [Code Generation] 正在生成研发协作错误码字典文档..."
    )

    add_custom_target(GenerateErrorAssets DEPENDS ${ALL_GENERATED_HEADERS} ${DICT_HEADER} ${DOCS_MD_OUT})
else()
    message(STATUS "Python3 未找到，跳过错误码/字典/文档代码生成")

    file(MAKE_DIRECTORY ${ERROR_GENERATED_DIR})
    file(WRITE "${ERROR_GENERATED_DIR}/error_dict.h"
        "#pragma once\n"
        "#include \"error_system/core/error_registry.h\"\n"
        "\n"
        "namespace error_system::core {\n"
        "    struct subsystem_module_registrar_t {\n"
        "        subsystem_module_registrar_t() {}\n"
        "    };\n"
        "    inline subsystem_module_registrar_t global_subsystem_module_registrar_{};\n"
        "}\n"
    )
endif()

set(SOURCE_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/duplicate_policy.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/error_context.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/error_context_initializer.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/error_context_serializer.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/error_registry.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/domain/subsystem_module_registry.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/plugin/error_router_plugin.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/plugin/plugin_registry.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/utils/file_utils.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/utils/json_lexer.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/utils/json_utils.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/utils/stack_trace_utils.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/utils/string_utils.cc
)

add_library(${PROJECT_NAME} ${SOURCE_FILES})
add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_17)
error_system_apply_project_options(${PROJECT_NAME})

if (BUILD_SHARED_LIBS)
    target_compile_definitions(${PROJECT_NAME} PRIVATE ERROR_SYSTEM_BUILDING_SHARED)
    target_compile_definitions(${PROJECT_NAME} PUBLIC ERROR_SYSTEM_USING_SHARED)
endif()

if(TARGET GenerateErrorAssets)
    add_dependencies(${PROJECT_NAME} GenerateErrorAssets)
endif()

include(GNUInstallDirs)

target_include_directories(${PROJECT_NAME}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${ERROR_GENERATED_DIR}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

if (ERROR_SYSTEM_ENABLE_STACKTRACE)
    target_compile_definitions(${PROJECT_NAME} PUBLIC ERROR_SYSTEM_ENABLE_STACKTRACE)
    if (WIN32)
        target_link_libraries(${PROJECT_NAME} PRIVATE dbghelp)
    elseif(UNIX AND NOT APPLE)
        target_link_libraries(${PROJECT_NAME} PRIVATE ${CMAKE_DL_LIBS})
    endif()
endif()

if (ERROR_SYSTEM_ENABLE_VALIDATION)
    target_compile_definitions(${PROJECT_NAME} PUBLIC ERROR_SYSTEM_ENABLE_VALIDATION)
endif()

if (ERROR_SYSTEM_ENABLE_LOCATION)
    target_compile_definitions(${PROJECT_NAME} PUBLIC ERROR_SYSTEM_ENABLE_LOCATION)
endif()

if(ERROR_SYSTEM_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(ERROR_SYSTEM_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

install(TARGETS ${PROJECT_NAME}
    EXPORT ${PROJECT_NAME}Targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} 
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}       
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}  
)

install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp" PATTERN "*.inl"
)

include(CMakePackageConfigHelpers)

if(Python3_FOUND)
    install(DIRECTORY script/script_py/
        DESTINATION ${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}/scripts
        FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
    )
endif()

install(FILES cmake/error_system_helpers.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
    VERSION ${VERSION_STRING}
    COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/error_system_config.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}"
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
)

install(EXPORT ${PROJECT_NAME}Targets
    FILE ${PROJECT_NAME}Targets.cmake
    NAMESPACE ${PROJECT_NAME}::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
)

if(NOT TARGET uninstall)
    add_custom_target(uninstall
        COMMAND ${CMAKE_COMMAND} -E echo "开始强制卸载文件..."
        COMMAND ${CMAKE_COMMAND} -D "MANIFEST=${CMAKE_CURRENT_BINARY_DIR}/install_manifest.txt" -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/uninstall_manifest.cmake"
        COMMAND ${CMAKE_COMMAND} -E echo "清理残留的空目录..."
        COMMAND ${CMAKE_COMMAND} -E rm -Rf ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME}
        COMMAND ${CMAKE_COMMAND} -E rm -Rf ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
        COMMAND ${CMAKE_COMMAND} -E rm -Rf ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Uninstalling the project and cleaning up directories..."
        VERBATIM
    )
endif()
