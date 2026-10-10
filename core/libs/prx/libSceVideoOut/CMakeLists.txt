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
if(BUILD_TESTING)
    set(videoOutFlipProperties RESOURCE_LOCK videoout_app0 ENVIRONMENT "SDL_VIDEODRIVER=offscreen")
    anyps5_add_library_test(video_out_flip_tests
        NAME video_out_flip_lifetime
        TIMEOUT 30
        SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/Flip.cpp
        LINK libSceVideoOut libSceAgcDriver libkernel libc
        RUNTIME libc
        INCLUDES
            ${CMAKE_SOURCE_DIR}/3rdparty/SDL2/include
            ${CMAKE_SOURCE_DIR}/core/shader/recompiler
            ${CMAKE_SOURCE_DIR}/3rdparty/Vulkan-Headers/include
        ARGS lifetime
        PROPERTIES ${videoOutFlipProperties}
    )
    set_target_properties(video_out_flip_tests PROPERTIES CXX_EXTENSIONS OFF)
    foreach(flipMode IN ITEMS decode controls)
        add_test(NAME video_out_flip_${flipMode} COMMAND video_out_flip_tests ${flipMode})
        anyps5_set_test_properties(video_out_flip_${flipMode} integration 30 SKIP_RETURN_CODE 77 ${videoOutFlipProperties})
        if(WIN32)
            set_tests_properties(video_out_flip_${flipMode} PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
        endif()
    endforeach()

    anyps5_add_library_test(pad_sdl_tests
        LABEL unit
        SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/tests/unit/PadSdl.cpp
        LINK SDL2-static libc
        RUNTIME libc
    )

    foreach(videoOutTest IN ITEMS wqhd_detection:WqhdDetection resolution_status:ResolutionStatus output_mode:OutputMode)
        string(REPLACE ":" ";" videoOutTest "${videoOutTest}")
        list(GET videoOutTest 0 videoOutTestName)
        list(GET videoOutTest 1 videoOutTestSource)
        anyps5_add_library_test(guest_video_out_${videoOutTestName}_tests
            CUSTOM_MAIN
            TIMEOUT 20
            SOURCES ${LIBS_INCLUDE_DIR}/tests/integration/GuestVideoOut${videoOutTestSource}.cpp
            LINK libSceVideoOut libSceAgcDriver libkernel libc
            RUNTIME libc
            PROPERTIES RESOURCE_LOCK videoout_app0
        )
    endforeach()
    if(WIN32)
        add_test(NAME guest_video_out_wqhd_detection_no_vulkan COMMAND guest_video_out_wqhd_detection_tests)
        anyps5_set_test_properties(guest_video_out_wqhd_detection_no_vulkan integration 20 SKIP_RETURN_CODE 77 RESOURCE_LOCK videoout_app0)
        set_tests_properties(guest_video_out_wqhd_detection_no_vulkan PROPERTIES
            ENVIRONMENT "VK_DRIVER_FILES=${CMAKE_CURRENT_BINARY_DIR}/missing_icd.json;VK_ICD_FILENAMES=${CMAKE_CURRENT_BINARY_DIR}/missing_icd.json"
            ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()

    anyps5_add_library_test(guest_buffer_reuse_tests
        LABEL unit
        SOURCES ${LIBS_INCLUDE_DIR}/tests/unit/GuestBufferReuse.cpp
        LINK libc
    )
endif()
