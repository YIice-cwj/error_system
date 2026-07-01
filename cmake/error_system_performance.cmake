include(CheckIPOSupported)

if(ERROR_SYSTEM_ENABLE_PGO_GENERATE AND ERROR_SYSTEM_ENABLE_PGO_USE)
    message(FATAL_ERROR "ERROR_SYSTEM_ENABLE_PGO_GENERATE and ERROR_SYSTEM_ENABLE_PGO_USE cannot both be ON")
endif()

function(error_system_apply_warnings target_name)
    if(MSVC)
        target_compile_options(${target_name} PRIVATE /W4)
        if(ERROR_SYSTEM_WARNINGS_AS_ERRORS)
            target_compile_options(${target_name} PRIVATE /WX)
        endif()
        return()
    endif()

    target_compile_options(${target_name} PRIVATE
        -Wall -Wextra -Wpedantic
        -Wshadow -Wconversion -Wsign-conversion
        -Wdouble-promotion -Wnull-dereference
        -Wformat=2 -Wnon-virtual-dtor
        -Wunused -Wunused-parameter -Wunused-variable -Wunused-function
    )
    if(ERROR_SYSTEM_WARNINGS_AS_ERRORS)
        target_compile_options(${target_name} PRIVATE -Werror)
    endif()
endfunction()

function(error_system_apply_lto target_name)
    if(NOT ERROR_SYSTEM_ENABLE_LTO)
        return()
    endif()

    check_ipo_supported(RESULT ipo_supported OUTPUT ipo_output)
    if(ipo_supported)
        set_property(TARGET ${target_name} PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
    else()
        message(WARNING "IPO/LTO is not supported for target ${target_name}: ${ipo_output}")
    endif()
endfunction()

function(error_system_apply_sanitizers target_name)
    if(MSVC)
        return()
    endif()

    set(sanitizer_flags "")
    if(ERROR_SYSTEM_ENABLE_ASAN)
        list(APPEND sanitizer_flags -fsanitize=address)
    endif()
    if(ERROR_SYSTEM_ENABLE_UBSAN)
        list(APPEND sanitizer_flags -fsanitize=undefined)
    endif()

    if(NOT sanitizer_flags)
        return()
    endif()

    target_compile_options(${target_name} PRIVATE ${sanitizer_flags} -fno-omit-frame-pointer)
    target_link_options(${target_name} PRIVATE ${sanitizer_flags})
endfunction()

function(error_system_apply_pgo target_name)
    if(MSVC)
        return()
    endif()

    if(ERROR_SYSTEM_ENABLE_PGO_GENERATE)
        target_compile_options(${target_name} PRIVATE -fprofile-generate)
        target_link_options(${target_name} PRIVATE -fprofile-generate)
    endif()

    if(ERROR_SYSTEM_ENABLE_PGO_USE)
        target_compile_options(${target_name} PRIVATE -fprofile-use -fprofile-correction)
        target_link_options(${target_name} PRIVATE -fprofile-use)
    endif()
endfunction()

function(error_system_apply_project_options target_name)
    error_system_apply_warnings(${target_name})
    error_system_apply_lto(${target_name})
    error_system_apply_sanitizers(${target_name})
    error_system_apply_pgo(${target_name})
endfunction()

function(error_system_apply_warnings_only target_name)
    error_system_apply_warnings(${target_name})
endfunction()
