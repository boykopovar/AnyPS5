if(BUILD_TESTING)
    if(WIN32 OR CMAKE_SYSTEM_NAME STREQUAL "Linux")
        add_test_executable(agc_driver_frame_capture_eop_tests tests/FrameCaptureEop.cpp Execution/src/FrameCapture.cpp)
        target_include_directories(agc_driver_frame_capture_eop_tests PRIVATE ${LIBS_INCLUDE_DIR} ${CMAKE_SOURCE_DIR}/core/shader/recompiler ${ANYPS5_VULKAN_HEADERS_INCLUDE_DIR} ${ANYPS5_SPIRV_HEADERS_INCLUDE_DIR})
        target_link_libraries(agc_driver_frame_capture_eop_tests PRIVATE libSceAgcDriver libkernel libc libzstd_static)
        set_target_properties(agc_driver_frame_capture_eop_tests PROPERTIES CXX_EXTENSIONS OFF)
        configure_windows_unwind(agc_driver_frame_capture_eop_tests)
        add_test(NAME agc_driver_frame_capture_eop COMMAND agc_driver_frame_capture_eop_tests)
        set_tests_properties(agc_driver_frame_capture_eop PROPERTIES
                ENVIRONMENT "ANYPS5_NO_SHADER_CACHE=1;APS5_FRAME_CAPTURE=${CMAKE_CURRENT_BINARY_DIR}/frame-capture-eop;APS5_FRAME_CAPTURE_AT_FLIP=1;APS5_FRAME_CAPTURE_FRAMES=3"
                TIMEOUT 120)
        if(WIN32)
            set_tests_properties(agc_driver_frame_capture_eop PROPERTIES ENVIRONMENT_MODIFICATION
                    "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
        endif()
    endif()
endif()
