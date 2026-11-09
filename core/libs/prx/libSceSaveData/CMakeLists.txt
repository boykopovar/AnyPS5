include(${CMAKE_CURRENT_SOURCE_DIR}/../libSceSaveData.native/SaveDataLibrary.cmake)
add_sce_savedata_library(libSceSaveData)
target_sources(libSceSaveData PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/InventoryExports.cpp)
