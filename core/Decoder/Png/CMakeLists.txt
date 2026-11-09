function(add_png_decoder target runtimeTarget)
    if(NOT TARGET "${runtimeTarget}")
        message(FATAL_ERROR "PNG decoder requires an existing runtime target: ${runtimeTarget}")
    endif()

    add_library(${target} STATIC ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/src/Png.cpp)
    target_include_directories(${target} PUBLIC ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/include)
    target_include_directories(${target} SYSTEM PRIVATE ${PROJECT_SOURCE_DIR}/3rdparty/stb)
    target_link_libraries(${target} PRIVATE ${runtimeTarget})
    set_target_properties(${target} PROPERTIES
        CXX_EXTENSIONS OFF
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
        POSITION_INDEPENDENT_CODE ON
    )

    if(BUILD_TESTING)
        add_test_executable(${target}_tests ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/tests/Png.cpp)
        target_include_directories(${target}_tests SYSTEM PRIVATE ${PROJECT_SOURCE_DIR}/3rdparty/stb)
        target_link_libraries(${target}_tests PRIVATE ${target})
        configure_windows_unwind(${target}_tests)
        add_test(NAME ${target} COMMAND ${target}_tests)
        if(WIN32)
            set_tests_properties(${target} PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:${runtimeTarget}>")
        endif()
    endif()
endfunction()
