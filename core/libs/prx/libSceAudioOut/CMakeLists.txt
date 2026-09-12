add_library(libSceAudioOut SHARED EXCLUDE_FROM_ALL
        ${CMAKE_CURRENT_SOURCE_DIR}/src/AudioOut.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/AudioOut2Context.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/AudioOut2Port.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/AudioOut2SpeakerArray.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/AudioOut2User.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/AudioOut2Mastering.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/AudioOut2System.cpp
)

target_include_directories(libSceAudioOut PRIVATE
        ${LIBS_INCLUDE_DIR}
        ${CMAKE_SOURCE_DIR}/3rdparty/SDL2/include
)
target_link_libraries(libSceAudioOut PRIVATE libc libkernel SDL2-static)
