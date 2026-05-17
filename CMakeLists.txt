cmake_minimum_required(VERSION 3.15)

set(PROJECT_NAME "error_system")

set(VERSION_MAJOR 1)
set(VERSION_MINOR 0)
set(VERSION_PATCH 0)
set(VERSION_STRING ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH})

set(DESCRIPTION_STRING "error_system_v${VERSION_STRING}: c++17 错误码系统，提供错误码的定义和使用")

project(${PROJECT_NAME} VERSION ${VERSION_STRING} DESCRIPTION ${DESCRIPTION_STRING} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON) 
set(CMAKE_CXX_EXTENSIONS OFF)

if (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    add_compile_options(/W4 /WX)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
    if (WIN32)
        add_compile_options(-g) 
        add_link_options(-Wl,--export-all-symbols)
    endif()
endif()

option(ERROR_SYSTEM_ENABLE_STACKTRACE "是否编译堆栈追踪功能 (关闭可减小体积，免链接 OS 底层库)" ON)
option(ERROR_SYSTEM_ENABLE_VALIDATION "是否编译严格的错误码验证器" ON)
option(ERROR_SYSTEM_ENABLE_LOCATION "是否编译源位置宏" ON)

find_package(Python3 COMPONENTS Interpreter REQUIRED)

set(ERROR_JSON_DIR "${CMAKE_SOURCE_DIR}/config/errors")
set(ERROR_GENERATED_DIR "${CMAKE_BINARY_DIR}/generated_errors/include")
set(PYTHON_SCRIPT "${CMAKE_SOURCE_DIR}/script/script_py/generate_error_codes.py")
file(GLOB ERROR_JSON_FILES "${ERROR_JSON_DIR}/*.json")
set(ALL_GENERATED_HEADERS "")

foreach(JSON_FILE ${ERROR_JSON_FILES})
    get_filename_component(BASENAME ${JSON_FILE} NAME_WE)
    set(GENERATED_HEADER "${ERROR_GENERATED_DIR}/${BASENAME}.h")
    
    add_custom_command(
        OUTPUT ${GENERATED_HEADER}
        COMMAND Python3::Interpreter ${PYTHON_SCRIPT} ${JSON_FILE} ${ERROR_GENERATED_DIR}
        DEPENDS ${JSON_FILE} ${PYTHON_SCRIPT}
        COMMENT "🔨 [Code Generation] 正在从 ${BASENAME}.json 生成 C++ 错误码头文件..."
    )
    list(APPEND ALL_GENERATED_HEADERS ${GENERATED_HEADER})
endforeach()

add_custom_target(GenerateErrorCodes DEPENDS ${ALL_GENERATED_HEADERS})

set(DICT_SCRIPT "${CMAKE_SOURCE_DIR}/script/script_py/generate_error_dict.py")
set(DICT_HEADER "${ERROR_GENERATED_DIR}/error_dict.h")

add_custom_command(
    OUTPUT ${DICT_HEADER}
    COMMAND Python3::Interpreter ${DICT_SCRIPT} ${ERROR_JSON_DIR} ${DICT_HEADER}
    DEPENDS ${ERROR_JSON_FILES} ${DICT_SCRIPT}
    COMMENT "🔨 [Code Generation] 正在生成 O(1) 极速错误字典..."
)

add_custom_target(GenerateErrorDict DEPENDS ${DICT_HEADER})

include_directories(${ERROR_GENERATED_DIR})

file(GLOB_RECURSE SOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

add_library(${PROJECT_NAME} ${SOURCE_FILES})
add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

add_dependencies(${PROJECT_NAME} GenerateErrorCodes GenerateErrorDict)

include(GNUInstallDirs)

target_include_directories(${PROJECT_NAME}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

if (ERROR_SYSTEM_ENABLE_STACKTRACE)
    target_compile_definitions(${PROJECT_NAME} PUBLIC ERROR_SYSTEM_ENABLE_STACKTRACE)
    if (WIN32)
        target_link_libraries(${PROJECT_NAME} PRIVATE dbghelp)
    endif()
endif()

if (ERROR_SYSTEM_ENABLE_VALIDATION)
    target_compile_definitions(${PROJECT_NAME} PUBLIC ERROR_SYSTEM_ENABLE_VALIDATION)
endif()

if (ERROR_SYSTEM_ENABLE_LOCATION)
    target_compile_definitions(${PROJECT_NAME} PUBLIC ERROR_SYSTEM_ENABLE_LOCATION)
endif()

option(ERROR_SYSTEM_BUILD_TESTS "Build tests for error_system" ON)
option(ERROR_SYSTEM_BUILD_EXAMPLES "Build examples for error_system" ON)

if(ERROR_SYSTEM_BUILD_TESTS)
    include(FetchContent)
    FetchContent_Declare(
      googletest
      GIT_REPOSITORY https://github.com/google/googletest.git
      GIT_TAG        v1.14.0
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    
    FetchContent_MakeAvailable(googletest)

    enable_testing()
    include(GoogleTest)
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
)

include(CMakePackageConfigHelpers)

install(DIRECTORY script/script_py/
    DESTINATION ${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}/scripts
    FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)

install(FILES cmake/ErrorSystemHelpers.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
    VERSION ${VERSION_STRING}
    COMPATIBILITY SameMajorVersion
)

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
    "include(\"\${CMAKE_CURRENT_LIST_DIR}/${PROJECT_NAME}Targets.cmake\")
    include(\"\${CMAKE_CURRENT_LIST_DIR}/ErrorSystemHelpers.cmake\")"
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
        COMMAND sh -c "[ -f install_manifest.txt ] && xargs rm -vf < install_manifest.txt || echo '⚠️ 未找到 install_manifest.txt，跳过文件逐一删除。'"
        COMMAND ${CMAKE_COMMAND} -E echo "清理残留的空目录..."
        COMMAND rm -rf ${CMAKE_INSTALL_PREFIX}/include/${PROJECT_NAME}
        COMMAND rm -rf ${CMAKE_INSTALL_PREFIX}/lib/cmake/${PROJECT_NAME}
        COMMAND rm -rf ${CMAKE_INSTALL_PREFIX}/share/${PROJECT_NAME}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Uninstalling the project and cleaning up directories..."
        VERBATIM
    )
endif()