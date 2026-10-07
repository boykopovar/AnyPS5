add_library(libSceNpUniversalDataSystem SHARED EXCLUDE_FROM_ALL
        ${CMAKE_CURRENT_SOURCE_DIR}/src/System.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Context.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Handle.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Event.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/EventPropertyObject.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/EventPropertyArray.cpp
)

target_include_directories(libSceNpUniversalDataSystem PRIVATE
        ${LIBS_INCLUDE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(libSceNpUniversalDataSystem PRIVATE libc)
configure_windows_unwind(libSceNpUniversalDataSystem)

if(MINGW)
    foreach(runtimeLinkFlag IN ITEMS -static-libgcc -static-libstdc++ -static)
        string(REPLACE "${runtimeLinkFlag}" "" CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")
    endforeach()
endif()

if(BUILD_TESTING)
    add_test_executable(guest_np_universal_data_system_tests ${LIBS_INCLUDE_DIR}/tests/GuestNpUniversalDataSystem.cpp)
    target_include_directories(guest_np_universal_data_system_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_np_universal_data_system_tests PRIVATE libSceNpUniversalDataSystem libc)
    configure_windows_unwind(guest_np_universal_data_system_tests)
    add_test(NAME guest_np_universal_data_system COMMAND guest_np_universal_data_system_tests)
    if(WIN32)
        set_tests_properties(guest_np_universal_data_system PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceNpUniversalDataSystem>")
    endif()
endif()
