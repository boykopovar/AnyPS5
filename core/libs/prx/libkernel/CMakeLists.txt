add_library(libkernel SHARED EXCLUDE_FROM_ALL
    ${CMAKE_CURRENT_SOURCE_DIR}/Export.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/DirectMemory/Export.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/DirectMemory/DirectMemory.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/DirectMemory/MemoryPool.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Pthread/Pthread.cpp
)

target_include_directories(libkernel PRIVATE ${LIBS_INCLUDE_DIR})
