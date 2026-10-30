add_library(libSceVideoOut SHARED EXCLUDE_FROM_ALL
        ${CMAKE_CURRENT_SOURCE_DIR}/Unimplemented.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/InventoryExports.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Event.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Buffer.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Output.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/VideoOutDriver.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/DisplayWindow.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/PadInput.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/PadStatusSdl.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Telemetry.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/MouseInput.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/KeyboardInput.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ImeTextInput.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/DialogInput.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/InputMapping.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/KeyboardMouse.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/HostHdrProbe.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../libkernel/Time/TimedWait.cpp
)

target_include_directories(libSceVideoOut PRIVATE
        ${LIBS_INCLUDE_DIR}
        ${ANYPS5_SDL2_INCLUDE_DIR}
        ${ANYPS5_VULKAN_HEADERS_INCLUDE_DIR}
        ${CMAKE_BINARY_DIR}/core/libs/prx/libSceAgcDriver/generated
)
add_dependencies(libSceVideoOut agc_trace_revision)
target_link_libraries(libSceVideoOut PRIVATE libSceAgcDriver libScePad libSceUserService libSceMouse libSceKeyboard libSceIme libc libkernel decoder_png SDL2-static)
if(WIN32)
    target_link_libraries(libSceVideoOut PRIVATE comctl32 dxgi)
endif()
set_target_properties(libSceVideoOut PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(libSceVideoOut)

foreach(runtimeLinkFlag IN ITEMS -static-libgcc -static-libstdc++ -static)
    string(REPLACE "${runtimeLinkFlag}" "" CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")
endforeach()
add_test_executable(video_out_mouse_tests EXCLUDE_FROM_ALL tests/MouseInput.cpp)
target_include_directories(video_out_mouse_tests PRIVATE ${LIBS_INCLUDE_DIR} ${ANYPS5_SDL2_INCLUDE_DIR})
target_link_libraries(video_out_mouse_tests PRIVATE libSceVideoOut libSceMouse libSceIme libc SDL2-static)
set_target_properties(video_out_mouse_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(video_out_mouse_tests)
if(BUILD_TESTING)
    set_target_properties(video_out_mouse_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME video_out_mouse COMMAND video_out_mouse_tests)
    if(WIN32)
        set_tests_properties(video_out_mouse PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceVideoOut>;PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceMouse>;PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceIme>;PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()

add_test_executable(video_out_dialog_input_tests EXCLUDE_FROM_ALL tests/DialogInput.cpp)
target_include_directories(video_out_dialog_input_tests PRIVATE ${LIBS_INCLUDE_DIR} ${ANYPS5_SDL2_INCLUDE_DIR})
target_link_libraries(video_out_dialog_input_tests PRIVATE libSceVideoOut libSceIme libc SDL2-static)
set_target_properties(video_out_dialog_input_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(video_out_dialog_input_tests)
if(BUILD_TESTING)
    set_target_properties(video_out_dialog_input_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME video_out_dialog_input COMMAND video_out_dialog_input_tests)
    if(WIN32)
        set_tests_properties(video_out_dialog_input PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceVideoOut>;PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceIme>;PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()

add_test_executable(video_out_flip_tests EXCLUDE_FROM_ALL tests/Flip.cpp)
target_include_directories(video_out_flip_tests PRIVATE ${LIBS_INCLUDE_DIR} ${ANYPS5_SDL2_INCLUDE_DIR} ${CMAKE_SOURCE_DIR}/core/shader/recompiler ${ANYPS5_VULKAN_HEADERS_INCLUDE_DIR})
target_link_libraries(video_out_flip_tests PRIVATE libSceVideoOut libSceAgcDriver libkernel libc)
set_target_properties(video_out_flip_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(video_out_flip_tests)
if(BUILD_TESTING)
    add_test_executable(video_out_output_mode_policy_tests tests/OutputModePolicy.cpp)
    target_include_directories(video_out_output_mode_policy_tests PRIVATE ${LIBS_INCLUDE_DIR} ${ANYPS5_SDL2_INCLUDE_DIR})
    target_link_libraries(video_out_output_mode_policy_tests PRIVATE libSceVideoOut libc libkernel)
    configure_windows_unwind(video_out_output_mode_policy_tests)
    add_test(NAME video_out_output_mode_policy COMMAND video_out_output_mode_policy_tests)
    set_tests_properties(video_out_output_mode_policy PROPERTIES TIMEOUT 20)
    if(WIN32)
        set_tests_properties(video_out_output_mode_policy PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()

    add_test_executable(guest_video_out_high_frame_rate_tests ${LIBS_INCLUDE_DIR}/tests/GuestVideoOutHighFrameRate.cpp)
    target_include_directories(guest_video_out_high_frame_rate_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_video_out_high_frame_rate_tests PRIVATE libSceVideoOut libSceAgcDriver libkernel libc)
    configure_windows_unwind(guest_video_out_high_frame_rate_tests)
    add_test(NAME guest_video_out_high_frame_rate COMMAND guest_video_out_high_frame_rate_tests)
    set_tests_properties(guest_video_out_high_frame_rate PROPERTIES TIMEOUT 20 SKIP_RETURN_CODE 77 RESOURCE_LOCK videoout_app0)
    if(WIN32)
        set_tests_properties(guest_video_out_high_frame_rate PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    if(WIN32)
        set(videoOutTestDriver "windows")
    else()
        set(videoOutTestDriver "offscreen")
    endif()
    set(ANYPS5_VIDEO_OUT_TEST_DRIVER "${videoOutTestDriver}" CACHE STRING
        "SDL video driver for VideoOut flip tests")

    foreach(flipMode IN ITEMS labels labels_main)
        add_test(NAME video_out_flip_${flipMode} COMMAND video_out_flip_tests ${flipMode})
        set_tests_properties(video_out_flip_${flipMode} PROPERTIES TIMEOUT 30 SKIP_RETURN_CODE 77 RESOURCE_LOCK videoout_app0 ENVIRONMENT "SDL_VIDEODRIVER=${ANYPS5_VIDEO_OUT_TEST_DRIVER}")
        if(WIN32)
            set_tests_properties(video_out_flip_${flipMode} PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
        endif()
    endforeach()

    set_target_properties(video_out_flip_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    foreach(flipMode IN ITEMS lifetime reopen decode controls overlay padstatus unopened ps4 resolution resolution_override fresh)
        if(flipMode STREQUAL "lifetime")
            add_test(NAME video_out_flip_${flipMode} COMMAND video_out_flip_tests)
        else()
            add_test(NAME video_out_flip_${flipMode} COMMAND video_out_flip_tests ${flipMode})
        endif()
        set_tests_properties(video_out_flip_${flipMode} PROPERTIES TIMEOUT 30 SKIP_RETURN_CODE 77 RESOURCE_LOCK videoout_app0 ENVIRONMENT "SDL_VIDEODRIVER=${ANYPS5_VIDEO_OUT_TEST_DRIVER}")
        if(WIN32)
            set_tests_properties(video_out_flip_${flipMode} PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
        endif()
    endforeach()
    foreach(flipMode IN ITEMS eop eop_main eop_aftergpu cpuflip)
        add_test(NAME video_out_flip_${flipMode} COMMAND video_out_flip_tests ${flipMode})
        set_tests_properties(video_out_flip_${flipMode} PROPERTIES TIMEOUT 30 SKIP_RETURN_CODE 77 RESOURCE_LOCK videoout_app0 ENVIRONMENT "SDL_VIDEODRIVER=${ANYPS5_VIDEO_OUT_TEST_DRIVER}")
        if(WIN32)
            set_tests_properties(video_out_flip_${flipMode} PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
        endif()
    endforeach()
    add_test(NAME video_out_flip_cpuflip_in_order COMMAND video_out_flip_tests cpuflip)
    set_tests_properties(video_out_flip_cpuflip_in_order PROPERTIES TIMEOUT 30 SKIP_RETURN_CODE 77 RESOURCE_LOCK videoout_app0 ENVIRONMENT "SDL_VIDEODRIVER=${ANYPS5_VIDEO_OUT_TEST_DRIVER};APS5_CPU_FLIP_IN_ORDER=1")
    if(WIN32)
        set_tests_properties(video_out_flip_cpuflip_in_order PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()

if(BUILD_TESTING AND NOT WIN32)
    add_test_executable(video_out_frame_dump_queue_tests tests/FrameDumpQueue.cpp)
    target_include_directories(video_out_frame_dump_queue_tests PRIVATE ${LIBS_INCLUDE_DIR} ${ANYPS5_SDL2_INCLUDE_DIR} ${ANYPS5_VULKAN_HEADERS_INCLUDE_DIR})
    target_link_libraries(video_out_frame_dump_queue_tests PRIVATE libSceVideoOut libSceAgcDriver libkernel libc)
    set_target_properties(video_out_frame_dump_queue_tests PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME video_out_frame_dump_queue COMMAND video_out_frame_dump_queue_tests)
    set_tests_properties(video_out_frame_dump_queue PROPERTIES TIMEOUT 30 SKIP_RETURN_CODE 77 RESOURCE_LOCK videoout_app0
        ENVIRONMENT "SDL_VIDEODRIVER=${ANYPS5_VIDEO_OUT_TEST_DRIVER};APS5_DUMP_FRAMES=4;APS5_DUMP_SCALE=4")
    add_test(NAME video_out_frame_dump_wait COMMAND video_out_frame_dump_queue_tests wait)
    set_tests_properties(video_out_frame_dump_wait PROPERTIES TIMEOUT 30 SKIP_RETURN_CODE 77 RESOURCE_LOCK videoout_app0
        ENVIRONMENT "SDL_VIDEODRIVER=${ANYPS5_VIDEO_OUT_TEST_DRIVER};APS5_DUMP_FRAMES=4;APS5_DUMP_SCALE=1")
    add_test(NAME video_out_frame_dump_cpu_queue COMMAND video_out_frame_dump_queue_tests)
    set_tests_properties(video_out_frame_dump_cpu_queue PROPERTIES TIMEOUT 30 SKIP_RETURN_CODE 77 RESOURCE_LOCK videoout_app0
        ENVIRONMENT "SDL_VIDEODRIVER=${ANYPS5_VIDEO_OUT_TEST_DRIVER};APS5_DUMP_FRAMES=4;APS5_DUMP_SCALE=4;APS5_NO_GPU_DUMP=1")
endif()

if(BUILD_TESTING)
    add_test_executable(pad_sdl_tests tests/PadSdl.cpp)
    target_include_directories(pad_sdl_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(pad_sdl_tests PRIVATE libSceVideoOut libScePad libSceIme libSceUserService libc SDL2-static)
    configure_windows_unwind(pad_sdl_tests)
    add_test(NAME pad_sdl COMMAND pad_sdl_tests)
    set_tests_properties(pad_sdl PROPERTIES SKIP_RETURN_CODE 77)
    if(WIN32)
        set_tests_properties(pad_sdl PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceVideoOut>;PATH=path_list_prepend:$<TARGET_FILE_DIR:libScePad>;PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceIme>;PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceUserService>;PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test_executable(pad_input_mapping_tests tests/InputMapping.cpp src/InputMapping.cpp)
    target_include_directories(pad_input_mapping_tests PRIVATE ${LIBS_INCLUDE_DIR} ${CMAKE_SOURCE_DIR}/3rdparty/SDL2/include)
    target_link_libraries(pad_input_mapping_tests PRIVATE SDL2-static libc)
    set_target_properties(pad_input_mapping_tests PROPERTIES CXX_EXTENSIONS OFF)
    configure_windows_unwind(pad_input_mapping_tests)
    add_test(NAME pad_input_mapping COMMAND pad_input_mapping_tests)
    if(WIN32)
        set_tests_properties(pad_input_mapping PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
    add_test_executable(pad_status_sdl_tests tests/PadStatusSdl.cpp src/PadStatusSdl.cpp)
    target_include_directories(pad_status_sdl_tests PRIVATE ${LIBS_INCLUDE_DIR} ${CMAKE_SOURCE_DIR}/3rdparty/SDL2/include)
    set_target_properties(pad_status_sdl_tests PROPERTIES CXX_EXTENSIONS OFF)
    configure_host_test_executable(pad_status_sdl_tests)
    add_test(NAME pad_status_sdl COMMAND pad_status_sdl_tests)
    add_test_executable(pad_output_pacing_tests tests/PadOutputPacing.cpp)
    target_include_directories(pad_output_pacing_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(pad_output_pacing_tests PRIVATE libc)
    configure_windows_unwind(pad_output_pacing_tests)
    add_test(NAME pad_output_pacing COMMAND pad_output_pacing_tests)
    if(WIN32)
        set_tests_properties(pad_output_pacing PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()

if(BUILD_TESTING)
    add_test_executable(pad_keyboard_mouse_tests tests/KeyboardMouse.cpp src/KeyboardMouse.cpp)
    target_include_directories(pad_keyboard_mouse_tests PRIVATE ${LIBS_INCLUDE_DIR} ${CMAKE_SOURCE_DIR}/3rdparty/SDL2/include)
    target_link_libraries(pad_keyboard_mouse_tests PRIVATE SDL2-static)
    if(MINGW)
        target_compile_options(pad_keyboard_mouse_tests PRIVATE -fasynchronous-unwind-tables)
        target_link_options(pad_keyboard_mouse_tests PRIVATE -static -static-libgcc -static-libstdc++)
    endif()
    add_test(NAME pad_keyboard_mouse COMMAND pad_keyboard_mouse_tests)
endif()

if(BUILD_TESTING)
    add_test_executable(guest_video_out_wqhd_detection_tests ${LIBS_INCLUDE_DIR}/tests/GuestVideoOutWqhdDetection.cpp)
    target_include_directories(guest_video_out_wqhd_detection_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_video_out_wqhd_detection_tests PRIVATE libSceVideoOut libSceAgcDriver libkernel libc)
    configure_windows_unwind(guest_video_out_wqhd_detection_tests)
    add_test(NAME guest_video_out_wqhd_detection COMMAND guest_video_out_wqhd_detection_tests)
    set_tests_properties(guest_video_out_wqhd_detection PROPERTIES TIMEOUT 20 SKIP_RETURN_CODE 77 RESOURCE_LOCK videoout_app0)
    if(WIN32)
        set_tests_properties(guest_video_out_wqhd_detection PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
        add_test(NAME guest_video_out_wqhd_detection_no_vulkan COMMAND guest_video_out_wqhd_detection_tests)
        set_tests_properties(guest_video_out_wqhd_detection_no_vulkan PROPERTIES TIMEOUT 20 SKIP_RETURN_CODE 77 RESOURCE_LOCK videoout_app0
            ENVIRONMENT "VK_DRIVER_FILES=${CMAKE_CURRENT_BINARY_DIR}/missing_icd.json;VK_ICD_FILENAMES=${CMAKE_CURRENT_BINARY_DIR}/missing_icd.json"
            ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()

if(BUILD_TESTING)
    add_test_executable(guest_video_out_resolution_status_tests ${LIBS_INCLUDE_DIR}/tests/GuestVideoOutResolutionStatus.cpp)
    target_include_directories(guest_video_out_resolution_status_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_video_out_resolution_status_tests PRIVATE libSceVideoOut libSceAgcDriver libkernel libc)
    configure_windows_unwind(guest_video_out_resolution_status_tests)
    add_test(NAME guest_video_out_resolution_status COMMAND guest_video_out_resolution_status_tests)
    set_tests_properties(guest_video_out_resolution_status PROPERTIES TIMEOUT 20 SKIP_RETURN_CODE 77 RESOURCE_LOCK videoout_app0)
    if(WIN32)
        set_tests_properties(guest_video_out_resolution_status PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()

if(BUILD_TESTING)
    add_test_executable(guest_video_out_output_mode_tests ${LIBS_INCLUDE_DIR}/tests/GuestVideoOutOutputMode.cpp)
    target_include_directories(guest_video_out_output_mode_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_video_out_output_mode_tests PRIVATE libSceVideoOut libSceAgcDriver libkernel libc)
    configure_windows_unwind(guest_video_out_output_mode_tests)
    add_test(NAME guest_video_out_output_mode COMMAND guest_video_out_output_mode_tests)
    set_tests_properties(guest_video_out_output_mode PROPERTIES TIMEOUT 20 SKIP_RETURN_CODE 77 RESOURCE_LOCK videoout_app0)
    if(WIN32)
        set_tests_properties(guest_video_out_output_mode PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()

    add_test_executable(guest_video_out_color_space_tests ${LIBS_INCLUDE_DIR}/tests/GuestVideoOutColorSpace.cpp)
    target_include_directories(guest_video_out_color_space_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_video_out_color_space_tests PRIVATE libSceVideoOut libSceAgcDriver libkernel libc)
    configure_windows_unwind(guest_video_out_color_space_tests)
    add_test(NAME guest_video_out_color_space COMMAND guest_video_out_color_space_tests)
    set_tests_properties(guest_video_out_color_space PROPERTIES TIMEOUT 20 SKIP_RETURN_CODE 77 RESOURCE_LOCK videoout_app0)
    if(WIN32)
        set_tests_properties(guest_video_out_color_space PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()

if(BUILD_TESTING)
    add_test_executable(guest_video_out_event_id_tests ${LIBS_INCLUDE_DIR}/tests/GuestVideoOutEventId.cpp)
    target_include_directories(guest_video_out_event_id_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_video_out_event_id_tests PRIVATE libSceVideoOut libkernel libc)
    configure_windows_unwind(guest_video_out_event_id_tests)
    add_test(NAME guest_video_out_event_id COMMAND guest_video_out_event_id_tests)
    set_tests_properties(guest_video_out_event_id PROPERTIES TIMEOUT 20)
    if(WIN32)
        set_tests_properties(guest_video_out_event_id PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()

if(BUILD_TESTING)
    add_test_executable(guest_video_out_hdr_tests ${LIBS_INCLUDE_DIR}/tests/GuestVideoOutHdr.cpp)
    target_include_directories(guest_video_out_hdr_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_video_out_hdr_tests PRIVATE libSceVideoOut libSceSystemService libSceAgcDriver libkernel libc)
    configure_windows_unwind(guest_video_out_hdr_tests)
    add_test(NAME guest_video_out_hdr COMMAND guest_video_out_hdr_tests)
    set_tests_properties(guest_video_out_hdr PROPERTIES TIMEOUT 20 SKIP_RETURN_CODE 77 RESOURCE_LOCK videoout_app0)
    if(WIN32)
        set_tests_properties(guest_video_out_hdr PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()

    add_test_executable(video_out_hdr_probe_tests tests/HdrProbe.cpp)
    target_include_directories(video_out_hdr_probe_tests PRIVATE ${LIBS_INCLUDE_DIR} ${ANYPS5_SDL2_INCLUDE_DIR})
    target_link_libraries(video_out_hdr_probe_tests PRIVATE libSceVideoOut libSceAgcDriver libc SDL2-static)
    if(WIN32)
        target_link_libraries(video_out_hdr_probe_tests PRIVATE dxgi)
    endif()
    set_target_properties(video_out_hdr_probe_tests PROPERTIES CXX_EXTENSIONS OFF)
    configure_windows_unwind(video_out_hdr_probe_tests)
    add_test(NAME video_out_hdr_probe COMMAND video_out_hdr_probe_tests)
    set_tests_properties(video_out_hdr_probe PROPERTIES TIMEOUT 20 SKIP_RETURN_CODE 77 ENVIRONMENT "SDL_VIDEODRIVER=${ANYPS5_VIDEO_OUT_TEST_DRIVER}")
    if(WIN32)
        set_tests_properties(video_out_hdr_probe PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()

    add_test_executable(video_out_hdr_present_tests tests/HdrPresent.cpp)
    target_include_directories(video_out_hdr_present_tests PRIVATE ${LIBS_INCLUDE_DIR} ${ANYPS5_SDL2_INCLUDE_DIR} ${ANYPS5_VULKAN_HEADERS_INCLUDE_DIR})
    target_link_libraries(video_out_hdr_present_tests PRIVATE libSceVideoOut libSceAgcDriver libkernel libc)
    set_target_properties(video_out_hdr_present_tests PROPERTIES CXX_EXTENSIONS OFF)
    configure_windows_unwind(video_out_hdr_present_tests)
    add_test(NAME video_out_hdr_present COMMAND video_out_hdr_present_tests)
    set_tests_properties(video_out_hdr_present PROPERTIES TIMEOUT 30 SKIP_RETURN_CODE 77 RESOURCE_LOCK videoout_app0)
    if(WIN32)
        set_tests_properties(video_out_hdr_present PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()

if(BUILD_TESTING)
    add_test_executable(guest_buffer_reuse_tests ${LIBS_INCLUDE_DIR}/tests/GuestBufferReuse.cpp)
    target_include_directories(guest_buffer_reuse_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_buffer_reuse_tests PRIVATE libc)
    configure_windows_unwind(guest_buffer_reuse_tests)
    add_test(NAME guest_buffer_reuse COMMAND guest_buffer_reuse_tests)
    if(WIN32)
        set_tests_properties(guest_buffer_reuse PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()

if(BUILD_TESTING)
    add_test_executable(video_out_telemetry_tests tests/Telemetry.cpp)
    target_include_directories(video_out_telemetry_tests PRIVATE ${LIBS_INCLUDE_DIR} ${ANYPS5_SDL2_INCLUDE_DIR})
    target_link_libraries(video_out_telemetry_tests PRIVATE libSceVideoOut libSceAgcDriver libkernel libc SDL2-static)
    set_target_properties(video_out_telemetry_tests PROPERTIES CXX_EXTENSIONS OFF)
    configure_windows_unwind(video_out_telemetry_tests)
    add_test(NAME video_out_telemetry COMMAND video_out_telemetry_tests)
    add_test(NAME video_out_telemetry_caption COMMAND video_out_telemetry_tests caption)
    set_tests_properties(video_out_telemetry PROPERTIES TIMEOUT 30)
    set_tests_properties(video_out_telemetry_caption PROPERTIES TIMEOUT 30 SKIP_RETURN_CODE 77 RESOURCE_LOCK videoout_app0 ENVIRONMENT "SDL_VIDEODRIVER=${ANYPS5_VIDEO_OUT_TEST_DRIVER}")
    if(WIN32)
        set_tests_properties(video_out_telemetry video_out_telemetry_caption PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
