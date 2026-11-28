function(add_sce_ces_library target)
    add_library(${target} SHARED EXCLUDE_FROM_ALL
        ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Export.cpp
        ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/Unicode.cpp)
    target_include_directories(${target} PRIVATE ${LIBS_INCLUDE_DIR})
    configure_windows_unwind(${target})
endfunction()
