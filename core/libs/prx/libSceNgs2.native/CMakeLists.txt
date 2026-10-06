add_library(libSceNgs2.native SHARED EXCLUDE_FROM_ALL
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Atrac9.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Custom.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Rack.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Render.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/System.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Unimplemented.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Voice.cpp
)
target_include_directories(libSceNgs2.native PRIVATE ${LIBS_INCLUDE_DIR} ${CMAKE_SOURCE_DIR}/3rdparty/LibAtrac9/C/src)
target_link_libraries(libSceNgs2.native PRIVATE atrac9)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_options(libSceNgs2.native PRIVATE LINKER:--exclude-libs,ALL)
endif()
set_target_properties(libSceNgs2.native PROPERTIES
        CXX_EXTENSIONS OFF
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
)
configure_windows_unwind(libSceNgs2.native)

if(MINGW)
    foreach(runtimeLinkFlag IN ITEMS -static-libgcc -static-libstdc++ -static)
        string(REPLACE "${runtimeLinkFlag}" "" CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")
    endforeach()
endif()

if(BUILD_TESTING)
    foreach(ngs2Test IN ITEMS Render Atrac9 Filter Custom)
        string(TOLOWER ${ngs2Test} testName)
        add_test_executable(ngs2_${testName}_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/Ngs2${ngs2Test}.cpp)
        target_include_directories(ngs2_${testName}_tests PRIVATE ${LIBS_INCLUDE_DIR} ${CMAKE_SOURCE_DIR}/3rdparty/LibAtrac9/C/src)
        target_link_libraries(ngs2_${testName}_tests PRIVATE libSceNgs2.native atrac9 libkernel libc)
        configure_windows_unwind(ngs2_${testName}_tests)
        add_test(NAME ngs2_${testName} COMMAND ngs2_${testName}_tests)
        if(WIN32)
            set_tests_properties(ngs2_${testName} PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceNgs2.native>")
        endif()
    endforeach()
endif()
