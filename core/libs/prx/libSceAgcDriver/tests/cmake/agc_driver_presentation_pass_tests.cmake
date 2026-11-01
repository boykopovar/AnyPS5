add_test_executable(agc_driver_presentation_pass_tests EXCLUDE_FROM_ALL
        tests/PresentationPass.cpp
)

target_include_directories(agc_driver_presentation_pass_tests PRIVATE
        ${LIBS_INCLUDE_DIR}
        ${CMAKE_SOURCE_DIR}/core/shader/recompiler
        ${ANYPS5_VULKAN_HEADERS_INCLUDE_DIR}
)

target_link_libraries(agc_driver_presentation_pass_tests PRIVATE
        libSceAgcDriver
        SDL2-static
        libc
)

set_target_properties(agc_driver_presentation_pass_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_driver_presentation_pass_tests)

if(BUILD_TESTING)
    set_target_properties(agc_driver_presentation_pass_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_driver_presentation_pass COMMAND agc_driver_presentation_pass_tests)
    set_tests_properties(agc_driver_presentation_pass PROPERTIES ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1" SKIP_RETURN_CODE 77 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    if(WIN32)
        set_tests_properties(agc_driver_presentation_pass PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
