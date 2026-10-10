add_library(libSceKeyboard SHARED EXCLUDE_FROM_ALL
    ${CMAKE_CURRENT_SOURCE_DIR}/Export.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/keyboard_impl.cpp
)
target_include_directories(libSceKeyboard PRIVATE ${LIBS_INCLUDE_DIR})
set_target_properties(libSceKeyboard PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(libSceKeyboard)

if(BUILD_TESTING)
    anyps5_add_library_test(keyboard_tests
        NAME keyboard_input
        SOURCES
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/Keyboard.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/../libSceVideoOut/src/KeyboardInput.cpp
        LINK libSceKeyboard libkernel libc
        INCLUDES ${CMAKE_SOURCE_DIR}/3rdparty/SDL2/include
    )
endif()
