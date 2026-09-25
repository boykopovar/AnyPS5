add_library(libSceVideoOut SHARED EXCLUDE_FROM_ALL
        ${CMAKE_CURRENT_SOURCE_DIR}/Unimplemented.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Event.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Buffer.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Output.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/VideoOutDriver.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/DisplayWindow.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/PadInput.cpp
)

target_include_directories(libSceVideoOut PRIVATE
        ${LIBS_INCLUDE_DIR}
        ${CMAKE_SOURCE_DIR}/3rdparty/SDL2/include
        ${CMAKE_SOURCE_DIR}/3rdparty/Vulkan-Headers/include
)
target_link_libraries(libSceVideoOut PRIVATE libSceAgcDriver libScePad libc libkernel SDL2-static)
if(WIN32)
    target_link_libraries(libSceVideoOut PRIVATE comctl32)
endif()
set_target_properties(libSceVideoOut PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(libSceVideoOut)

foreach(runtimeLinkFlag IN ITEMS -static-libgcc -static-libstdc++ -static)
    string(REPLACE "${runtimeLinkFlag}" "" CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")
endforeach()
add_executable(video_out_flip_tests EXCLUDE_FROM_ALL tests/Flip.cpp)
target_include_directories(video_out_flip_tests PRIVATE ${LIBS_INCLUDE_DIR} ${CMAKE_SOURCE_DIR}/3rdparty/SDL2/include)
target_link_libraries(video_out_flip_tests PRIVATE libSceVideoOut libSceAgcDriver libkernel libc)
set_target_properties(video_out_flip_tests PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(video_out_flip_tests)
