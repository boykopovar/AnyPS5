add_library(libSceMouse SHARED EXCLUDE_FROM_ALL
    ${CMAKE_CURRENT_SOURCE_DIR}/Export.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/mouse_impl.cpp
)
target_include_directories(libSceMouse PRIVATE ${LIBS_INCLUDE_DIR})
set_target_properties(libSceMouse PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(libSceMouse)

if(BUILD_TESTING)
    anyps5_add_library_test(mouse_tests
        NAME mouse_input
        SOURCES
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/Mouse.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/../libSceVideoOut/src/MouseInput.cpp
        LINK libSceMouse libkernel libc
        INCLUDES ${CMAKE_SOURCE_DIR}/3rdparty/SDL2/include
    )
endif()
