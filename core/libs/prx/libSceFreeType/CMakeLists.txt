add_library(libSceFreeType SHARED EXCLUDE_FROM_ALL src/Geometry.cpp
        Export.cpp
        src/Outline.cpp
        src/Charmap.cpp
        src/FaceInfo.cpp
        src/GuestObjects.cpp
        src/Names.cpp
)
target_include_directories(libSceFreeType PRIVATE ${LIBS_INCLUDE_DIR})
target_link_libraries(libSceFreeType PRIVATE freetype)
target_sources(libSceFreeType PRIVATE src/SizeRequest.cpp)

if(BUILD_TESTING)
    add_test_executable(guest_freetype_request_size_tests ${LIBS_INCLUDE_DIR}/tests/GuestFreeTypeRequestSize.cpp)
    target_include_directories(guest_freetype_request_size_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_freetype_request_size_tests PRIVATE libSceFreeType freetype libc)
    configure_windows_unwind(guest_freetype_request_size_tests)
    add_test(NAME guest_freetype_request_size COMMAND guest_freetype_request_size_tests)
    if(WIN32)
        set_tests_properties(guest_freetype_request_size PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceFreeType>")
    endif()
endif()

target_sources(libSceFreeType PRIVATE src/List.cpp)
if(MINGW OR CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_options(libSceFreeType PRIVATE LINKER:--exclude-libs,ALL)
endif()
configure_windows_unwind(libSceFreeType)

if(BUILD_TESTING)
    add_test_executable(guest_freetype_outline_tests ${LIBS_INCLUDE_DIR}/tests/GuestFreeTypeOutline.cpp)
    target_include_directories(guest_freetype_outline_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_freetype_outline_tests PRIVATE libSceFreeType freetype libc)
    configure_windows_unwind(guest_freetype_outline_tests)
    add_test(NAME guest_freetype_outline COMMAND guest_freetype_outline_tests)
    add_test_executable(guest_freetype_charmap_info_tests ${LIBS_INCLUDE_DIR}/tests/GuestFreeTypeCharmapInfo.cpp)
    target_include_directories(guest_freetype_charmap_info_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_freetype_charmap_info_tests PRIVATE libSceFreeType freetype libc)
    configure_windows_unwind(guest_freetype_charmap_info_tests)
    add_test(NAME guest_freetype_charmap_info COMMAND guest_freetype_charmap_info_tests)
    add_test_executable(guest_freetype_tests ${LIBS_INCLUDE_DIR}/tests/GuestFreeType.cpp)
    target_include_directories(guest_freetype_tests PRIVATE ${LIBS_INCLUDE_DIR})
    if(Python3_Interpreter_FOUND)
        add_test_executable(guest_freetype_geometry_tests ${LIBS_INCLUDE_DIR}/tests/GuestFreeTypeGeometry.cpp)
        target_include_directories(guest_freetype_geometry_tests PRIVATE ${LIBS_INCLUDE_DIR})
        target_link_libraries(guest_freetype_geometry_tests PRIVATE libSceFreeType freetype libc)
        configure_windows_unwind(guest_freetype_geometry_tests)
        add_test(NAME guest_freetype_geometry COMMAND ${Python3_EXECUTABLE}
            ${LIBS_INCLUDE_DIR}/tests/test_guest_freetype_geometry.py $<TARGET_FILE:guest_freetype_geometry_tests>)
        set_tests_properties(guest_freetype_geometry PROPERTIES TIMEOUT 30)
        if(WIN32)
            set_tests_properties(guest_freetype_geometry PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceFreeType>")
        endif()
    endif()
    target_link_libraries(guest_freetype_tests PRIVATE libSceFreeType freetype libc)
    configure_windows_unwind(guest_freetype_tests)
    add_test(NAME guest_freetype COMMAND guest_freetype_tests)
    add_test_executable(guest_freetype_outline_mutations_tests ${LIBS_INCLUDE_DIR}/tests/GuestFreeTypeOutlineMutations.cpp)
    target_include_directories(guest_freetype_outline_mutations_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_freetype_outline_mutations_tests PRIVATE libSceFreeType freetype libc)
    configure_windows_unwind(guest_freetype_outline_mutations_tests)
    add_test(NAME guest_freetype_outline_mutations COMMAND guest_freetype_outline_mutations_tests)
    add_test_executable(guest_freetype_face_info_tests ${LIBS_INCLUDE_DIR}/tests/GuestFreeTypeFaceInfo.cpp)
    target_include_directories(guest_freetype_face_info_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_freetype_face_info_tests PRIVATE libSceFreeType freetype libc)
    configure_windows_unwind(guest_freetype_face_info_tests)
    add_test(NAME guest_freetype_face_info COMMAND guest_freetype_face_info_tests)
    add_test_executable(guest_freetype_charmap_tests ${LIBS_INCLUDE_DIR}/tests/GuestFreeTypeCharmap.cpp)
    target_include_directories(guest_freetype_charmap_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_freetype_charmap_tests PRIVATE libSceFreeType freetype libc)
    configure_windows_unwind(guest_freetype_charmap_tests)
    add_test(NAME guest_freetype_charmap COMMAND guest_freetype_charmap_tests)
    add_test_executable(guest_freetype_names_tests ${LIBS_INCLUDE_DIR}/tests/GuestFreeTypeNames.cpp)
    target_include_directories(guest_freetype_names_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_freetype_names_tests PRIVATE libSceFreeType freetype libc)
    configure_windows_unwind(guest_freetype_names_tests)
    add_test(NAME guest_freetype_names COMMAND guest_freetype_names_tests)
    add_test_executable(guest_freetype_sfnt_names_tests ${LIBS_INCLUDE_DIR}/tests/GuestFreeTypeSfntNames.cpp)
    target_include_directories(guest_freetype_sfnt_names_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_freetype_sfnt_names_tests PRIVATE libSceFreeType freetype libc)
    configure_windows_unwind(guest_freetype_sfnt_names_tests)
    add_test(NAME guest_freetype_sfnt_names COMMAND guest_freetype_sfnt_names_tests)
    if(WIN32)
        set_tests_properties(guest_freetype_sfnt_names PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceFreeType>")
        set_tests_properties(guest_freetype_names PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceFreeType>")
        set_tests_properties(guest_freetype PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceFreeType>")
        set_tests_properties(guest_freetype_charmap PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceFreeType>")
        set_tests_properties(guest_freetype_outline PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceFreeType>")
        set_tests_properties(guest_freetype_charmap_info PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceFreeType>")
        set_tests_properties(guest_freetype_outline_mutations PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceFreeType>")
        set_tests_properties(guest_freetype_face_info PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceFreeType>")
    endif()
endif()

if(BUILD_TESTING)
    add_test_executable(guest_freetype_list_tests ${LIBS_INCLUDE_DIR}/tests/GuestFreeTypeList.cpp)
    target_include_directories(guest_freetype_list_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_freetype_list_tests PRIVATE libSceFreeType freetype libc)
    configure_windows_unwind(guest_freetype_list_tests)
    add_test(NAME guest_freetype_list COMMAND guest_freetype_list_tests)
    set_tests_properties(guest_freetype_list PROPERTIES TIMEOUT 20)
    if(WIN32)
        set_tests_properties(guest_freetype_list PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceFreeType>")
    endif()
endif()
