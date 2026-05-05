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

file(GLOB_RECURSE SOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

add_library(${PROJECT_NAME} ${SOURCE_FILES})
add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

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

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
    VERSION ${VERSION_STRING}
    COMPATIBILITY SameMajorVersion
)

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
    "include(\"\${CMAKE_CURRENT_LIST_DIR}/${PROJECT_NAME}Targets.cmake\")\n"
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