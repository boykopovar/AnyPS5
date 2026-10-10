add_library(libSceCdlgPlayerReview SHARED EXCLUDE_FROM_ALL Export.cpp)
target_include_directories(libSceCdlgPlayerReview PRIVATE ${LIBS_INCLUDE_DIR})
configure_windows_unwind(libSceCdlgPlayerReview)

if(MINGW)
    foreach(runtimeLinkFlag IN ITEMS -static-libgcc -static-libstdc++ -static)
        string(REPLACE "${runtimeLinkFlag}" "" CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")
    endforeach()
endif()

if(BUILD_TESTING)
    anyps5_add_library_test(guest_player_review_dialog_tests
        SOURCES ${LIBS_INCLUDE_DIR}/tests/integration/GuestPlayerReviewDialog.cpp
        LINK libSceCdlgPlayerReview libc
    )
endif()
