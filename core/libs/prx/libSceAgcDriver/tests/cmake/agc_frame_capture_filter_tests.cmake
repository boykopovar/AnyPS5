add_executable(agc_frame_capture_filter EXCLUDE_FROM_ALL
        tools/FrameCaptureFilter.cpp
        tools/FrameCaptureFilterMain.cpp
        Execution/src/FrameCapture.cpp
)
target_include_directories(agc_frame_capture_filter PRIVATE ${LIBS_INCLUDE_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(agc_frame_capture_filter PRIVATE libc libzstd_static)
set_target_properties(agc_frame_capture_filter PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_frame_capture_filter)

if(BUILD_TESTING)
    add_test_executable(agc_frame_capture_filter_tests tests/FrameCaptureFilter.cpp tools/FrameCaptureFilter.cpp Execution/src/FrameCapture.cpp)
    target_include_directories(agc_frame_capture_filter_tests PRIVATE ${LIBS_INCLUDE_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(agc_frame_capture_filter_tests PRIVATE libc libzstd_static)
    set_target_properties(agc_frame_capture_filter_tests PROPERTIES CXX_EXTENSIONS OFF)
    configure_windows_unwind(agc_frame_capture_filter_tests)
    add_test(NAME agc_frame_capture_filter COMMAND agc_frame_capture_filter_tests)
    set_tests_properties(agc_frame_capture_filter PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(agc_frame_capture_filter PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    if(Python3_Interpreter_FOUND)
        add_test(NAME frame_capture_minimize COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_SOURCE_DIR}/tools/tests/test_frame_capture_minimize.py)
        set_tests_properties(frame_capture_minimize PROPERTIES TIMEOUT 20)
    endif()
endif()
