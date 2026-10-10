add_library(libSceNpManager SHARED EXCLUDE_FROM_ALL Export.cpp)
target_include_directories(libSceNpManager PRIVATE ${LIBS_INCLUDE_DIR})
configure_windows_unwind(libSceNpManager)

if(MINGW)
    foreach(runtimeLinkFlag IN ITEMS -static-libgcc -static-libstdc++ -static)
        string(REPLACE "${runtimeLinkFlag}" "" CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")
    endforeach()
endif()

if(BUILD_TESTING)
    anyps5_add_library_test(guest_np_reachability_tests
        SOURCES ${LIBS_INCLUDE_DIR}/tests/integration/GuestNpReachability.cpp
        LINK libSceNpManager libc
    )
    anyps5_add_library_test(guest_np_manager_tests
        SOURCES ${LIBS_INCLUDE_DIR}/tests/integration/GuestNpManager.cpp
        LINK libSceNpManager libc
    )
endif()
