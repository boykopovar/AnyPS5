function(ensure_glslim_glslang)
    if(TARGET glslang)
        return()
    endif()

    set(BUILD_SHARED_LIBS OFF)
    set(BUILD_EXTERNAL OFF)
    set(GLSLANG_TESTS OFF)
    set(GLSLANG_ENABLE_INSTALL OFF)
    set(ENABLE_GLSLANG_BINARIES ON)
    set(ENABLE_SPIRV ON)
    set(ENABLE_SPVREMAPPER OFF)
    set(ENABLE_OPT OFF)

    add_subdirectory(
        "${CMAKE_SOURCE_DIR}/3rdparty/glslang"
        "${CMAKE_BINARY_DIR}/3rdparty/glslang"
        EXCLUDE_FROM_ALL
    )
endfunction()

ensure_glslim_glslang()

add_library(libSceGLSlimVSH SHARED EXCLUDE_FROM_ALL
    Export.cpp
    src/State.cpp
    src/Egl.cpp
    src/GlState.cpp
    src/Objects.cpp
    src/Shaders.cpp
    src/Images.cpp)
target_include_directories(libSceGLSlimVSH PRIVATE ${LIBS_INCLUDE_DIR} ${ANYPS5_GLSLANG_INCLUDE_DIRS})
target_link_libraries(libSceGLSlimVSH PRIVATE libc glslang)
if(MINGW OR CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_options(libSceGLSlimVSH PRIVATE LINKER:--exclude-libs,ALL)
elseif(APPLE)
    target_link_options(libSceGLSlimVSH PRIVATE
        "LINKER:-exported_symbol,*_nid_*"
        "LINKER:-exported_symbol,_scePiglet*"
        "LINKER:-exported_symbol,_sceSlimgl*")
endif()
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
    add_test_executable(guest_glslim_ipc_returns_tests tests/IpcReturns.cpp)
    target_include_directories(guest_glslim_ipc_returns_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_glslim_ipc_returns_tests PRIVATE libSceGLSlimVSH libc)
    configure_windows_unwind(guest_glslim_ipc_returns_tests)
    add_test(NAME guest_glslim_ipc_returns COMMAND guest_glslim_ipc_returns_tests)
    if(WIN32)
        set_tests_properties(guest_glslim_ipc_returns PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceGLSlimVSH>")
    endif()
    add_test_executable(guest_glslim_eglgetconfigs_tests tests/EglGetConfigs.cpp)
    target_include_directories(guest_glslim_eglgetconfigs_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_glslim_eglgetconfigs_tests PRIVATE libSceGLSlimVSH libc)
    configure_windows_unwind(guest_glslim_eglgetconfigs_tests)
    add_test(NAME guest_glslim_eglgetconfigs COMMAND guest_glslim_eglgetconfigs_tests)
    if(WIN32)
        set_tests_properties(guest_glslim_eglgetconfigs PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceGLSlimVSH>")
    endif()
    add_test_executable(guest_glslim_query_surface_tests tests/EglQuerySurface.cpp)
    target_include_directories(guest_glslim_query_surface_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_glslim_query_surface_tests PRIVATE libSceGLSlimVSH libc)
    configure_windows_unwind(guest_glslim_query_surface_tests)
    add_test(NAME guest_glslim_query_surface COMMAND guest_glslim_query_surface_tests)
    set_tests_properties(guest_glslim_query_surface PROPERTIES TIMEOUT 20)
    if(WIN32)
        set_tests_properties(guest_glslim_query_surface PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceGLSlimVSH>")
    endif()
    add_test_executable(guest_glslim_attributes_tests tests/EglAttributes.cpp)
    target_include_directories(guest_glslim_attributes_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_glslim_attributes_tests PRIVATE libSceGLSlimVSH libc)
    configure_windows_unwind(guest_glslim_attributes_tests)
    add_test(NAME guest_glslim_attributes COMMAND guest_glslim_attributes_tests)
    set_tests_properties(guest_glslim_attributes PROPERTIES TIMEOUT 20)
    if(WIN32)
        set_tests_properties(guest_glslim_attributes PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceGLSlimVSH>")
    endif()
    add_test_executable(guest_glslim_shaders_tests tests/Shaders.cpp)
    target_include_directories(guest_glslim_shaders_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_glslim_shaders_tests PRIVATE libSceGLSlimVSH libc)
    configure_windows_unwind(guest_glslim_shaders_tests)
    add_test(NAME guest_glslim_shaders COMMAND guest_glslim_shaders_tests)
    set_tests_properties(guest_glslim_shaders PROPERTIES TIMEOUT 20)
    if(WIN32)
        set_tests_properties(guest_glslim_shaders PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceGLSlimVSH>")
    endif()
endif()
