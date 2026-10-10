include(${CMAKE_CURRENT_SOURCE_DIR}/../libSceNgs2.native/Ngs2Library.cmake)
add_sce_ngs2_library(libSceNgs2)

if(BUILD_TESTING)
    anyps5_add_library_test(ngs2_core_render_tests
        CUSTOM_MAIN
        SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../libSceNgs2.native/tests/integration/Ngs2Render.cpp
        LINK libSceNgs2 atrac9 libkernel libc
        RUNTIME libSceNgs2
        INCLUDES ${CMAKE_SOURCE_DIR}/3rdparty/LibAtrac9/C/src
    )
endif()
