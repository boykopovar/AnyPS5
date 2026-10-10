include(${CMAKE_CURRENT_SOURCE_DIR}/AvPlayerLibrary.cmake)
add_sce_avplayer_library(libSceAvPlayer)

if(BUILD_TESTING)
    foreach(runtimeLinkFlag IN ITEMS -static-libgcc -static-libstdc++ -static)
        string(REPLACE "${runtimeLinkFlag}" "" CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")
    endforeach()
    add_test_executable(guest_avplayer_tests ${LIBS_INCLUDE_DIR}/tests/GuestAvPlayer.cpp)
    target_include_directories(guest_avplayer_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_avplayer_tests PRIVATE libSceAvPlayer libc)
    configure_windows_unwind(guest_avplayer_tests)
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/guest_avplayer)
    add_test(NAME guest_avplayer COMMAND guest_avplayer_tests)
    set_tests_properties(guest_avplayer PROPERTIES
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/guest_avplayer
        TIMEOUT 60)
    if(WIN32)
        set_tests_properties(guest_avplayer PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceAvPlayer>")
    endif()
endif()
