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
        set(decoderTestProperties)
        if(WIN32)
            set(decoderTestProperties ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:${runtimeTarget}>")
        endif()
        anyps5_add_test(${target}_tests
            NAME ${target}
            LABEL unit
            SOURCES ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/tests/unit/PngTests.cpp
            LINK ${target}
            PROPERTIES ${decoderTestProperties}
        )
        target_include_directories(${target}_tests SYSTEM PRIVATE ${PROJECT_SOURCE_DIR}/3rdparty/stb)
        configure_windows_unwind(${target}_tests)
    endif()
endfunction()
