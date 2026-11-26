add_library(libSceAjmi SHARED EXCLUDE_FROM_ALL ${CMAKE_CURRENT_SOURCE_DIR}/Export.cpp)
target_include_directories(libSceAjmi PRIVATE ${LIBS_INCLUDE_DIR})
target_link_libraries(libSceAjmi PRIVATE libSceAjm.native)
configure_windows_unwind(libSceAjmi)

if(BUILD_TESTING)
    add_test_executable(guest_ajmi_inline_guest_pointers_tests ${LIBS_INCLUDE_DIR}/tests/GuestAjmiInlineGuestPointers.cpp)
    target_include_directories(guest_ajmi_inline_guest_pointers_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_ajmi_inline_guest_pointers_tests PRIVATE libSceAjmi libc)
    configure_windows_unwind(guest_ajmi_inline_guest_pointers_tests)
    add_test(NAME guest_ajmi_inline_guest_pointers COMMAND guest_ajmi_inline_guest_pointers_tests)
    set_tests_properties(guest_ajmi_inline_guest_pointers PROPERTIES TIMEOUT 20)
    if(WIN32)
        set_tests_properties(guest_ajmi_inline_guest_pointers PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceAjmi>;PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()

    add_test_executable(guest_ajmi_batch_aliases_tests ${LIBS_INCLUDE_DIR}/tests/GuestAjmiBatchAliases.cpp)
    target_include_directories(guest_ajmi_batch_aliases_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_ajmi_batch_aliases_tests PRIVATE ${CMAKE_DL_LIBS})
    if(WIN32)
        target_link_libraries(guest_ajmi_batch_aliases_tests PRIVATE libc)
    endif()
    configure_windows_unwind(guest_ajmi_batch_aliases_tests)
    set(ajmiAliasModuleDir ${CMAKE_CURRENT_BINARY_DIR}/guest_ajmi_batch_alias_modules)
    set(ajmiAliasPatchedModule ${ajmiAliasModuleDir}/libSceAjmi.prx)
    set(ajmiAliasKernel ${ajmiAliasModuleDir}/libkernel.prx)
    set(ajmiAliasLibc ${ajmiAliasModuleDir}/libc.prx)
    set(ajmiAliasNative ${ajmiAliasModuleDir}/libSceAjm.native.prx)
    add_custom_command(
        OUTPUT ${ajmiAliasPatchedModule} ${ajmiAliasKernel} ${ajmiAliasLibc} ${ajmiAliasNative}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${ajmiAliasModuleDir}
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:libSceAjmi> ${ajmiAliasPatchedModule}
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:libkernel> ${ajmiAliasKernel}
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:libc> ${ajmiAliasLibc}
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:libSceAjm.native> ${ajmiAliasNative}
        COMMAND $<TARGET_FILE:nid_patcher> libSceAjmi --preserve-export-list ${LIBC_EXPORT_LIST} ${ajmiAliasPatchedModule}
        COMMAND $<TARGET_FILE:nid_patcher> libkernel --preserve-export-list ${LIBC_EXPORT_LIST} ${ajmiAliasKernel}
        COMMAND $<TARGET_FILE:nid_patcher> libc ${ajmiAliasLibc}
        COMMAND $<TARGET_FILE:nid_patcher> libSceAjm.native --preserve-export-list ${LIBC_EXPORT_LIST} ${ajmiAliasNative}
        DEPENDS nid_patcher libSceAjmi libSceAjm.native libkernel libc ${LIBC_EXPORT_LIST}
        VERBATIM)
    add_custom_target(guest_ajmi_batch_alias_libraries DEPENDS ${ajmiAliasPatchedModule} ${ajmiAliasKernel} ${ajmiAliasLibc} ${ajmiAliasNative})
    add_dependencies(guest_ajmi_batch_aliases_tests guest_ajmi_batch_alias_libraries)
    foreach(variant IN ITEMS unpatched nid)
        if(variant STREQUAL "nid")
            set(ajmiAliasModule ${ajmiAliasPatchedModule})
            set(ajmiAliasSearchDir ${ajmiAliasModuleDir})
        else()
            set(ajmiAliasModule ${UNPATCHED_DIR}/libSceAjmi.prx)
            set(ajmiAliasSearchDir ${UNPATCHED_DIR})
        endif()
        foreach(operation IN ITEMS inline run control split)
            add_test(NAME guest_ajmi_batch_${operation}_${variant} COMMAND guest_ajmi_batch_aliases_tests ${ajmiAliasModule} ${operation} ${variant})
            set_tests_properties(guest_ajmi_batch_${operation}_${variant} PROPERTIES TIMEOUT 20)
            if(WIN32)
                set_tests_properties(guest_ajmi_batch_${operation}_${variant} PROPERTIES ENVIRONMENT_MODIFICATION
                    "PATH=path_list_prepend:${ajmiAliasSearchDir}")
            elseif(APPLE)
                set_tests_properties(guest_ajmi_batch_${operation}_${variant} PROPERTIES ENVIRONMENT_MODIFICATION
                    "DYLD_FALLBACK_LIBRARY_PATH=path_list_prepend:${ajmiAliasSearchDir}")
            else()
                set_tests_properties(guest_ajmi_batch_${operation}_${variant} PROPERTIES ENVIRONMENT_MODIFICATION
                    "LD_LIBRARY_PATH=path_list_prepend:${ajmiAliasSearchDir}")
            endif()
        endforeach()
    endforeach()
endif()

include(Parsers.cmake)
include(LifecycleAliases.cmake)
include(Submission.cmake)
