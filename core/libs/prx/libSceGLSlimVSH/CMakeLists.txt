add_library(libSceGLSlimVSH SHARED EXCLUDE_FROM_ALL
    Export.cpp
    src/State.cpp
    src/Egl.cpp
    src/GlState.cpp
    src/Objects.cpp
    src/Images.cpp)
target_include_directories(libSceGLSlimVSH PRIVATE ${LIBS_INCLUDE_DIR})
configure_windows_unwind(libSceGLSlimVSH)

if(BUILD_TESTING)
    add_test_executable(guest_glslim_state_tests tests/State.cpp)
    target_include_directories(guest_glslim_state_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_glslim_state_tests PRIVATE libSceGLSlimVSH libc)
    configure_windows_unwind(guest_glslim_state_tests)
    add_test(NAME guest_glslim_state COMMAND guest_glslim_state_tests)
    set_tests_properties(guest_glslim_state PROPERTIES TIMEOUT 20)
    if(WIN32)
        set_tests_properties(guest_glslim_state PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceGLSlimVSH>")
    endif()
endif()
