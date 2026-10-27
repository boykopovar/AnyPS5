include(${CMAKE_CURRENT_SOURCE_DIR}/../libSceAudiodec/AudiodecLibrary.cmake)
add_sce_audiodec_library(libSceAudiodec.native)
target_sources(libSceAudiodec.native PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Export.cpp)
