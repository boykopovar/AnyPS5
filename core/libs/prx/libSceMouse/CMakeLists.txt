add_library(libSceMouse SHARED EXCLUDE_FROM_ALL
    ${CMAKE_CURRENT_SOURCE_DIR}/Export.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/mouse_impl.cpp
)
target_include_directories(libSceMouse PRIVATE ${LIBS_INCLUDE_DIR})
set_target_properties(libSceMouse PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(libSceMouse)

if(BUILD_TESTING)
    add_executable(mouse_tests
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/Mouse.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../libSceVideoOut/src/MouseInput.cpp
    )
    target_include_directories(mouse_tests PRIVATE ${LIBS_INCLUDE_DIR} ${CMAKE_SOURCE_DIR}/3rdparty/SDL2/include)
    target_link_libraries(mouse_tests PRIVATE libSceMouse libkernel libc)
    configure_windows_unwind(mouse_tests)
    add_test(NAME mouse_input COMMAND mouse_tests)
    if(WIN32)
        set_tests_properties(mouse_input PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceMouse>")
    endif()
endif()
