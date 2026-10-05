add_library(anyps5_input STATIC KeyboardMapping.cpp AtomicFile.cpp ControllerMapping.cpp)
target_include_directories(anyps5_input PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/core/libs)
target_link_libraries(anyps5_input PUBLIC SDL2-static)
set_target_properties(anyps5_input PROPERTIES POSITION_INDEPENDENT_CODE ON CXX_VISIBILITY_PRESET hidden)
if(BUILD_TESTING)
    add_test_executable(input_configuration_tests tests/Configuration.cpp)
    target_link_libraries(input_configuration_tests PRIVATE anyps5_input)
    add_test(NAME input_configuration COMMAND input_configuration_tests)
endif()

set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/3rdparty/imgui)
add_library(anyps5_input_editor STATIC
    InputEditor.cpp
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdlrenderer2.cpp
)
target_include_directories(anyps5_input_editor PRIVATE ${IMGUI_DIR} ${IMGUI_DIR}/backends)
target_link_libraries(anyps5_input_editor PUBLIC anyps5_input)
set_target_properties(anyps5_input_editor PROPERTIES POSITION_INDEPENDENT_CODE ON CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN ON)
add_executable(anyps5-input-config main.cpp)
target_link_libraries(anyps5-input-config PRIVATE anyps5_input_editor)

if(BUILD_TESTING)
    add_test_executable(input_controller_tests tests/Controllers.cpp)
    target_link_libraries(input_controller_tests PRIVATE anyps5_input)
    add_test(NAME input_controllers COMMAND input_controller_tests)
    add_test_executable(input_runtime_tests tests/Runtime.cpp ${CMAKE_SOURCE_DIR}/core/libs/prx/libSceVideoOut/src/PadInput.cpp)
    target_link_libraries(input_runtime_tests PRIVATE anyps5_input)
    add_test(NAME input_runtime COMMAND input_runtime_tests)
    set_tests_properties(input_runtime PROPERTIES ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_JOYSTICK_HIDAPI=0;SDL_JOYSTICK_RAWINPUT=0;SDL_JOYSTICK_DIRECTINPUT=0;SDL_JOYSTICK_WGI=0;SDL_XINPUT_ENABLED=0")
    add_test_executable(input_editor_tests tests/Editor.cpp)
    target_link_libraries(input_editor_tests PRIVATE anyps5_input_editor)
    add_test(NAME input_editor COMMAND input_editor_tests)
    set_tests_properties(input_editor PROPERTIES ENVIRONMENT "SDL_VIDEODRIVER=dummy")
endif()
