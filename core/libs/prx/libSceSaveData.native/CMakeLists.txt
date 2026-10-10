add_library(libSceSaveData.native SHARED EXCLUDE_FROM_ALL Export.cpp)
target_include_directories(libSceSaveData.native PRIVATE ${LIBS_INCLUDE_DIR})
configure_windows_unwind(libSceSaveData.native)

if(BUILD_TESTING)
    foreach(saveDataTest IN ITEMS search:Search delete:Delete mount_status:MountStatus ps4:Ps4)
        string(REPLACE ":" ";" saveDataTest "${saveDataTest}")
        list(GET saveDataTest 0 saveDataTestName)
        list(GET saveDataTest 1 saveDataTestSource)
        anyps5_add_library_test(guest_savedata_${saveDataTestName}_tests
            SOURCES ${LIBS_INCLUDE_DIR}/tests/integration/GuestSaveData${saveDataTestSource}.cpp
            LINK libSceSaveData.native libc
        )
    endforeach()
endif()
