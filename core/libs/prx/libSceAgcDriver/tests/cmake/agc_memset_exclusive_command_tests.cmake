add_test_executable(agc_memset_exclusive_command_tests EXCLUDE_FROM_ALL
    tests/AgcMemsetCommands.cpp
    ../libSceAgc/Command/src/Memset.cpp
    ../libSceAgc/Command/src/Packet.cpp
    ../libSceAgc/Cb/src/Registers.cpp
    ../libSceAgc/Shader/src/CreateShader.cpp
    ../libSceAgc/Shader/src/ShaderUtils.cpp)
target_include_directories(agc_memset_exclusive_command_tests PRIVATE ${LIBS_INCLUDE_DIR})
target_link_libraries(agc_memset_exclusive_command_tests PRIVATE libc libSceAgcDriver libkernel)
configure_windows_unwind(agc_memset_exclusive_command_tests)
if(BUILD_TESTING)
    set_target_properties(agc_memset_exclusive_command_tests PROPERTIES EXCLUDE_FROM_ALL OFF)
    add_test(NAME agc_memset_exclusive_commands COMMAND agc_memset_exclusive_command_tests)
    set_tests_properties(agc_memset_exclusive_commands PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(agc_memset_exclusive_commands PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()
