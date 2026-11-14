add_library(libSceAjmi SHARED EXCLUDE_FROM_ALL ${CMAKE_CURRENT_SOURCE_DIR}/Export.cpp)
target_include_directories(libSceAjmi PRIVATE ${LIBS_INCLUDE_DIR})
configure_windows_unwind(libSceAjmi)

if(BUILD_TESTING)
    add_test_executable(guest_ajmi_batch_aliases_tests ${LIBS_INCLUDE_DIR}/tests/GuestAjmiBatchAliases.cpp)
    target_include_directories(guest_ajmi_batch_aliases_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_ajmi_batch_aliases_tests PRIVATE libSceAjmi libc ${CMAKE_DL_LIBS})
    configure_windows_unwind(guest_ajmi_batch_aliases_tests)
    set(ajmiAliasModuleDir ${CMAKE_CURRENT_BINARY_DIR}/guest_ajmi_batch_alias_modules)
    set(ajmiAliasPatchedModule ${ajmiAliasModuleDir}/libSceAjmi.prx)
    set(ajmiAliasKernel ${ajmiAliasModuleDir}/libkernel.prx)
    set(ajmiAliasLibc ${ajmiAliasModuleDir}/libc.prx)
    add_custom_command(
        OUTPUT ${ajmiAliasPatchedModule} ${ajmiAliasKernel} ${ajmiAliasLibc}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${ajmiAliasModuleDir}
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:libSceAjmi> ${ajmiAliasPatchedModule}
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:libkernel> ${ajmiAliasKernel}
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:libc> ${ajmiAliasLibc}
        COMMAND $<TARGET_FILE:nid_patcher> libSceAjmi --preserve-export-list ${LIBC_EXPORT_LIST} ${ajmiAliasPatchedModule}
        COMMAND $<TARGET_FILE:nid_patcher> libkernel --preserve-export-list ${LIBC_EXPORT_LIST} ${ajmiAliasKernel}
        COMMAND $<TARGET_FILE:nid_patcher> libc ${ajmiAliasLibc}
        DEPENDS nid_patcher libSceAjmi libkernel libc ${LIBC_EXPORT_LIST}
        VERBATIM)
    add_custom_target(guest_ajmi_batch_alias_libraries DEPENDS ${ajmiAliasPatchedModule} ${ajmiAliasKernel} ${ajmiAliasLibc})
    add_dependencies(guest_ajmi_batch_aliases_tests guest_ajmi_batch_alias_libraries)
    foreach(variant IN ITEMS unpatched nid)
        if(variant STREQUAL "nid")
            set(ajmiAliasModule ${ajmiAliasPatchedModule})
        else()
            set(ajmiAliasModule ${UNPATCHED_DIR}/libSceAjmi.prx)
        endif()
        foreach(operation IN ITEMS inline run)
            add_test(NAME guest_ajmi_batch_${operation}_${variant} COMMAND guest_ajmi_batch_aliases_tests ${ajmiAliasModule} ${operation} ${variant})
            set_tests_properties(guest_ajmi_batch_${operation}_${variant} PROPERTIES TIMEOUT 20)
            if(WIN32)
                set_tests_properties(guest_ajmi_batch_${operation}_${variant} PROPERTIES ENVIRONMENT_MODIFICATION
                    "PATH=path_list_prepend:${UNPATCHED_DIR};PATH=path_list_prepend:${ajmiAliasModuleDir}")
            elseif(APPLE)
                set_tests_properties(guest_ajmi_batch_${operation}_${variant} PROPERTIES ENVIRONMENT_MODIFICATION
                    "DYLD_FALLBACK_LIBRARY_PATH=path_list_prepend:${UNPATCHED_DIR};DYLD_FALLBACK_LIBRARY_PATH=path_list_prepend:${ajmiAliasModuleDir}")
            else()
                set_tests_properties(guest_ajmi_batch_${operation}_${variant} PROPERTIES ENVIRONMENT_MODIFICATION
                    "LD_LIBRARY_PATH=path_list_prepend:${UNPATCHED_DIR};LD_LIBRARY_PATH=path_list_prepend:${ajmiAliasModuleDir}")
            endif()
        endforeach()
    endforeach()
endif()
