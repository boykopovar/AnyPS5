add_library(libScePlayerInvitationDialog SHARED EXCLUDE_FROM_ALL libScePlayerInvitationDialog.cpp)
target_include_directories(libScePlayerInvitationDialog PRIVATE ${LIBS_INCLUDE_DIR})
configure_windows_unwind(libScePlayerInvitationDialog)

if(BUILD_TESTING)
    anyps5_add_library_test(player_invitation_dialog_tests
        SOURCES ${LIBS_INCLUDE_DIR}/tests/integration/PlayerInvitationDialog.cpp
        LINK libScePlayerInvitationDialog libc
        RUNTIME libc
    )
endif()
