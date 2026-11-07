set(fontconfigSource "${PROJECT_SOURCE_DIR}/3rdparty/fontconfig")
set(fontconfigData "${CMAKE_CURRENT_BINARY_DIR}/FontconfigData.hpp")
install(FILES "${fontconfigSource}/COPYING" "${CMAKE_CURRENT_SOURCE_DIR}/Unicode-LICENSE.txt"
    DESTINATION licenses/fontconfig)
add_custom_command(OUTPUT "${fontconfigData}"
    COMMAND ${CMAKE_COMMAND} "-DFONTCONFIG_SOURCE=${fontconfigSource}" "-DOUTPUT=${fontconfigData}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/GenerateData.cmake"
    DEPENDS GenerateData.cmake
        "${fontconfigSource}/fc-case/CaseFolding.txt"
        "${fontconfigSource}/fc-lang/meson.build"
        "${fontconfigSource}/src/fcobjs.h"
        "${fontconfigSource}/fontconfig/fontconfig.h"
    VERBATIM)
add_library(libfontconfig SHARED EXCLUDE_FROM_ALL Export.cpp Sets.cpp Objects.cpp "${fontconfigData}")
target_include_directories(libfontconfig PRIVATE ${LIBS_INCLUDE_DIR} "${CMAKE_CURRENT_BINARY_DIR}")
configure_windows_unwind(libfontconfig)

if(BUILD_TESTING)
    add_test_executable(guest_fontconfig_tests ${LIBS_INCLUDE_DIR}/tests/GuestFontconfig.cpp)
    target_include_directories(guest_fontconfig_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_fontconfig_tests PRIVATE libfontconfig libc)
    configure_windows_unwind(guest_fontconfig_tests)
    add_test(NAME guest_fontconfig COMMAND guest_fontconfig_tests)
    set_tests_properties(guest_fontconfig PROPERTIES TIMEOUT 20)
    if(WIN32)
        set_tests_properties(guest_fontconfig PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libfontconfig>")
    endif()
    add_test_executable(guest_fontconfig_sets_tests ${LIBS_INCLUDE_DIR}/tests/GuestFontconfigSets.cpp Sets.cpp)
    target_include_directories(guest_fontconfig_sets_tests PRIVATE ${LIBS_INCLUDE_DIR})
    configure_windows_unwind(guest_fontconfig_sets_tests)
    add_test(NAME guest_fontconfig_sets COMMAND guest_fontconfig_sets_tests)
    set_tests_properties(guest_fontconfig_sets PROPERTIES TIMEOUT 20)

    add_test_executable(guest_fontconfig_objects_tests ${LIBS_INCLUDE_DIR}/tests/GuestFontconfigObjects.cpp)
    target_include_directories(guest_fontconfig_objects_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_fontconfig_objects_tests PRIVATE libfontconfig libc)
    configure_windows_unwind(guest_fontconfig_objects_tests)
    add_test(NAME guest_fontconfig_objects COMMAND guest_fontconfig_objects_tests)
    set_tests_properties(guest_fontconfig_objects PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(guest_fontconfig_objects PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libfontconfig>")
    endif()
endif()
