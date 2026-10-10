include(${CMAKE_CURRENT_SOURCE_DIR}/Ngs2Library.cmake)
add_sce_ngs2_library(libSceNgs2.native)

if(BUILD_TESTING)
    foreach(ngs2Test IN ITEMS Render Atrac9 Filter Custom Parse Stream UserFx)
        string(TOLOWER ${ngs2Test} testName)
        set(ngs2TestOptions)
        if(ngs2Test STREQUAL "Render")
            set(ngs2TestOptions CUSTOM_MAIN)
        endif()
        anyps5_add_library_test(ngs2_${testName}_tests ${ngs2TestOptions}
            SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/Ngs2${ngs2Test}.cpp
            LINK libSceNgs2.native atrac9 libkernel libc
            INCLUDES ${CMAKE_SOURCE_DIR}/3rdparty/LibAtrac9/C/src
        )
    endforeach()
endif()
