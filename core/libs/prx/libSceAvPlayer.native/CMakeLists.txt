include(${CMAKE_CURRENT_SOURCE_DIR}/../libSceAvPlayer/AvPlayerLibrary.cmake)
add_sce_avplayer_library(libSceAvPlayer.native)
target_sources(libSceAvPlayer.native PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Export.cpp)
set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/Export.cpp PROPERTIES COMPILE_OPTIONS -fvisibility=default)
