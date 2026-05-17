macro(error_system_generate_codes)
    set(options)
    set(oneValueArgs TARGET JSON_DIR)
    set(multiValueArgs)
    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(NOT ARG_TARGET)
        message(FATAL_ERROR "❌ [Error System SDK] 必须指定 TARGET 参数！")
    endif()
    if(NOT ARG_JSON_DIR)
        message(FATAL_ERROR "❌ [Error System SDK] 必须指定 JSON_DIR 参数！")
    endif()

    # 🔒 绝对路径强行死锁
    set(PYTHON_SCRIPT   "${CMAKE_INSTALL_PREFIX}/share/error_system/scripts/generate_error_codes.py")
    set(DICT_SCRIPT     "${CMAKE_INSTALL_PREFIX}/share/error_system/scripts/generate_error_dict.py")
    set(DOCS_SCRIPT     "${CMAKE_INSTALL_PREFIX}/share/error_system/scripts/generate_error_docs.py")

    set(OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated_errors/include")
    set(DOCS_OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated_errors/docs")

    file(GLOB JSON_FILES "${ARG_JSON_DIR}/*.json")

    set(BIZ_HEADERS "")

    foreach(JSON_FILE ${JSON_FILES})
        get_filename_component(BASENAME ${JSON_FILE} NAME_WE)
        set(GENERATED_HEADER "${OUTPUT_DIR}/${BASENAME}.h")
        
        add_custom_command(
            OUTPUT ${GENERATED_HEADER}
            COMMAND ${Python3_EXECUTABLE} ${PYTHON_SCRIPT} ${JSON_FILE} ${OUTPUT_DIR}
            DEPENDS ${JSON_FILE} ${PYTHON_SCRIPT}
            COMMENT "🔨 [Error System SDK] 正在从 ${BASENAME}.json 生成错误码头文件"
        )
        list(APPEND BIZ_HEADERS ${GENERATED_HEADER})
    endforeach()

    set(DICT_OUT "${OUTPUT_DIR}/error_dict.h")
    add_custom_command(
        OUTPUT ${DICT_OUT}
        COMMAND ${Python3_EXECUTABLE} ${DICT_SCRIPT} ${ARG_JSON_DIR} ${DICT_OUT}
        DEPENDS ${JSON_FILES} ${DICT_SCRIPT}
        COMMENT "🔨 [Error System SDK] 正在为 ${ARG_TARGET} 生成 O(1) 嵌入式字典"
    )

    set(DOCS_MD_OUT "${DOCS_OUT_DIR}/error_dictionary.md")
    add_custom_command(
        OUTPUT ${DOCS_MD_OUT}
        COMMAND ${Python3_EXECUTABLE} ${DOCS_SCRIPT} ${ARG_JSON_DIR} ${DOCS_MD_OUT}
        DEPENDS ${JSON_FILES} ${DOCS_SCRIPT}
        COMMENT "📝 [Error System SDK] 正在为 ${ARG_TARGET} 生成研发协作错误码字典文档"
    )

    set(GEN_TARGET_NAME "Generate_${ARG_TARGET}_ErrorAssets")
    add_custom_target(${GEN_TARGET_NAME} DEPENDS ${BIZ_HEADERS} ${DICT_OUT} ${DOCS_MD_OUT})
    
    add_dependencies(${ARG_TARGET} ${GEN_TARGET_NAME})

    # 5. 自动配置好头文件搜索路径
    target_include_directories(${ARG_TARGET} PUBLIC ${OUTPUT_DIR})

endmacro()