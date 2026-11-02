add_test_executable(agc_performance_overlay_statistics_tests EXCLUDE_FROM_ALL
        tests/PerformanceOverlayStatistics.cpp
)
target_include_directories(agc_performance_overlay_statistics_tests PRIVATE
        ${LIBS_INCLUDE_DIR}
        ${CMAKE_SOURCE_DIR}/core/shader/recompiler
        ${ANYPS5_VULKAN_HEADERS_INCLUDE_DIR}
)
target_link_libraries(agc_performance_overlay_statistics_tests PRIVATE
        libSceAgcDriver
        libc
)
set_target_properties(agc_performance_overlay_statistics_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_performance_overlay_statistics_tests)

add_test_executable(agc_keyboard_overlay_tests EXCLUDE_FROM_ALL
        tests/KeyboardOverlay.cpp
        ${CMAKE_SOURCE_DIR}/core/libs/prx/libSceImeDialog/src/PadKeyboard.cpp
)
target_include_directories(agc_keyboard_overlay_tests PRIVATE
        ${LIBS_INCLUDE_DIR}
        ${CMAKE_SOURCE_DIR}/core/shader/recompiler
        ${ANYPS5_VULKAN_HEADERS_INCLUDE_DIR}
)
target_link_libraries(agc_keyboard_overlay_tests PRIVATE
        libSceAgcDriver
        libc
)
set_target_properties(agc_keyboard_overlay_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_keyboard_overlay_tests)

add_test_executable(agc_pad_status_overlay_tests EXCLUDE_FROM_ALL
        tests/PadStatusOverlay.cpp
)
target_include_directories(agc_pad_status_overlay_tests PRIVATE
        ${LIBS_INCLUDE_DIR}
        ${CMAKE_SOURCE_DIR}/core/shader/recompiler
        ${ANYPS5_VULKAN_HEADERS_INCLUDE_DIR}
)
target_link_libraries(agc_pad_status_overlay_tests PRIVATE
        libSceAgcDriver
        libc
)
set_target_properties(agc_pad_status_overlay_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_pad_status_overlay_tests)

add_test_executable(agc_performance_overlay_tests EXCLUDE_FROM_ALL
        tests/PerformanceOverlay.cpp
)
target_include_directories(agc_performance_overlay_tests PRIVATE
        ${LIBS_INCLUDE_DIR}
        ${CMAKE_SOURCE_DIR}/core/shader/recompiler
        ${ANYPS5_VULKAN_HEADERS_INCLUDE_DIR}
)
target_link_libraries(agc_performance_overlay_tests PRIVATE
        libSceAgcDriver
        SDL2-static
        libc
)
set_target_properties(agc_performance_overlay_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(agc_performance_overlay_tests)

if(BUILD_TESTING)
    set_target_properties(agc_performance_overlay_statistics_tests agc_keyboard_overlay_tests agc_pad_status_overlay_tests agc_performance_overlay_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_performance_overlay_statistics COMMAND agc_performance_overlay_statistics_tests)
    add_test(NAME agc_keyboard_overlay COMMAND agc_keyboard_overlay_tests)
    add_test(NAME agc_pad_status_overlay COMMAND agc_pad_status_overlay_tests)
    add_test(NAME agc_pad_status_overlay_shared COMMAND agc_pad_status_overlay_tests default)
    add_test(NAME agc_pad_status_overlay_events COMMAND agc_pad_status_overlay_tests events)
    add_test(NAME agc_pad_status_overlay_invalid COMMAND agc_pad_status_overlay_tests invalid)
    set_tests_properties(agc_pad_status_overlay agc_pad_status_overlay_shared agc_pad_status_overlay_events agc_pad_status_overlay_invalid PROPERTIES TIMEOUT 30)
    foreach(padStatusCase IN ITEMS shared:unset: events:set:1 invalid:set:2)
        string(REGEX REPLACE "^([a-z]+):(.*)$" "\\1" padStatusName "${padStatusCase}")
        string(REGEX REPLACE "^([a-z]+):(.*)$" "\\2" padStatusValue "${padStatusCase}")
        set(padStatusModification "ANYPS5_PAD_STATUS=${padStatusValue}")
        if(WIN32)
            list(APPEND padStatusModification "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
        endif()
        set_tests_properties(agc_pad_status_overlay_${padStatusName} PROPERTIES ENVIRONMENT_MODIFICATION "${padStatusModification}")
    endforeach()
    add_test(NAME agc_performance_overlay COMMAND agc_performance_overlay_tests)
    add_test(NAME agc_performance_overlay_device_selection COMMAND agc_performance_overlay_tests device-selection)
    add_test(NAME agc_performance_overlay_device_selection_cpu COMMAND agc_performance_overlay_tests device-selection-cpu)
    set_tests_properties(agc_performance_overlay agc_performance_overlay_device_selection PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30)
    set_tests_properties(agc_performance_overlay_device_selection_cpu PROPERTIES TIMEOUT 30)
    if(ANYPS5_ENABLE_ASAN)
        set_tests_properties(agc_performance_overlay PROPERTIES TIMEOUT 120)
    endif()
    if(WIN32)
        set_tests_properties(agc_performance_overlay_statistics agc_keyboard_overlay agc_pad_status_overlay agc_performance_overlay agc_performance_overlay_device_selection agc_performance_overlay_device_selection_cpu PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
