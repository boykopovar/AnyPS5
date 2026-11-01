add_library(libSceVrHand SHARED EXCLUDE_FROM_ALL Export.cpp)
target_include_directories(libSceVrHand PRIVATE ${LIBS_INCLUDE_DIR})
configure_windows_unwind(libSceVrHand)

if(BUILD_TESTING)
    add_test_executable(guest_vr_hand_tests ${LIBS_INCLUDE_DIR}/tests/GuestVrHand.cpp)
    target_include_directories(guest_vr_hand_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_vr_hand_tests PRIVATE libSceVrHand)
    add_test(NAME guest_vr_hand COMMAND guest_vr_hand_tests)
    if(WIN32)
        set_tests_properties(guest_vr_hand PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceVrHand>")
    endif()
endif()
