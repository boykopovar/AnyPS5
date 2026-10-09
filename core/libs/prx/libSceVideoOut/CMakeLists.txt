add_library(libSceVideoOut SHARED EXCLUDE_FROM_ALL
        ${CMAKE_CURRENT_SOURCE_DIR}/Unimplemented.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Event.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Buffer.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Output.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/VideoOutDriver.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/DisplayWindow.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/PadInput.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/MouseInput.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/KeyboardInput.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/InputMapping.cpp
)

target_include_directories(libSceVideoOut PRIVATE
        ${LIBS_INCLUDE_DIR}
        ${CMAKE_SOURCE_DIR}/3rdparty/SDL2/include
        ${CMAKE_SOURCE_DIR}/3rdparty/Vulkan-Headers/include
)
target_link_libraries(libSceVideoOut PRIVATE libSceAgcDriver libScePad libSceMouse libSceKeyboard libc libkernel decoder_png SDL2-static)
if(WIN32)
    target_link_libraries(libSceVideoOut PRIVATE comctl32)
endif()
set_target_properties(libSceVideoOut PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(libSceVideoOut)

foreach(runtimeLinkFlag IN ITEMS -static-libgcc -static-libstdc++ -static)
    string(REPLACE "${runtimeLinkFlag}" "" CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")
endforeach()
add_test_executable(video_out_flip_tests EXCLUDE_FROM_ALL tests/Flip.cpp)
target_include_directories(video_out_flip_tests PRIVATE ${LIBS_INCLUDE_DIR} ${CMAKE_SOURCE_DIR}/3rdparty/SDL2/include ${CMAKE_SOURCE_DIR}/core/shader/recompiler ${CMAKE_SOURCE_DIR}/3rdparty/Vulkan-Headers/include)
target_link_libraries(video_out_flip_tests PRIVATE libSceVideoOut libSceAgcDriver libkernel libc)
set_target_properties(video_out_flip_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(video_out_flip_tests)
if(BUILD_TESTING)
    set_target_properties(video_out_flip_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    foreach(flipMode IN ITEMS lifetime reopen decode controls overlay)
        if(flipMode STREQUAL "lifetime")
            add_test(NAME video_out_flip_${flipMode} COMMAND video_out_flip_tests)
        else()
            add_test(NAME video_out_flip_${flipMode} COMMAND video_out_flip_tests ${flipMode})
        endif()
        set_tests_properties(video_out_flip_${flipMode} PROPERTIES TIMEOUT 30 SKIP_RETURN_CODE 77 RESOURCE_LOCK videoout_app0 ENVIRONMENT "SDL_VIDEODRIVER=offscreen")
        if(WIN32)
            set_tests_properties(video_out_flip_${flipMode} PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
        endif()
    endforeach()
endif()

if(BUILD_TESTING)
    add_test_executable(pad_sdl_tests tests/PadSdl.cpp)
    target_link_libraries(pad_sdl_tests PRIVATE SDL2-static)
    add_test(NAME pad_sdl COMMAND pad_sdl_tests)
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
endif()
if(BUILD_TESTING)
    add_test_executable(guest_video_out_vrr_tests ${LIBS_INCLUDE_DIR}/tests/GuestVideoOutVrr.cpp)
    target_include_directories(guest_video_out_vrr_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_video_out_vrr_tests PRIVATE libSceVideoOut libSceAgcDriver libkernel libc)
    configure_windows_unwind(guest_video_out_vrr_tests)
    add_test(NAME guest_video_out_vrr COMMAND guest_video_out_vrr_tests)
    set_tests_properties(guest_video_out_vrr PROPERTIES TIMEOUT 20 SKIP_RETURN_CODE 77)
    if(WIN32)
        set_tests_properties(guest_video_out_vrr PROPERTIES ENVIRONMENT_MODIFICATION
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
