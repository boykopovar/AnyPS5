include(${CMAKE_CURRENT_SOURCE_DIR}/../libSceNgs2.native/Ngs2Library.cmake)
add_sce_ngs2_library(libSceNgs2)

if(BUILD_TESTING)
    add_test_executable(ngs2_core_render_tests ${CMAKE_CURRENT_SOURCE_DIR}/../libSceNgs2.native/tests/Ngs2Render.cpp)
    target_include_directories(ngs2_core_render_tests PRIVATE ${LIBS_INCLUDE_DIR} ${CMAKE_SOURCE_DIR}/3rdparty/LibAtrac9/C/src)
    target_link_libraries(ngs2_core_render_tests PRIVATE libSceNgs2 atrac9 libkernel libc)
    configure_windows_unwind(ngs2_core_render_tests)
    add_test(NAME ngs2_core_render COMMAND ngs2_core_render_tests)
    if(WIN32)
        set_tests_properties(ngs2_core_render PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceNgs2>")
    endif()
endif()
