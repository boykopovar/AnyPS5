add_library(libkernel SHARED EXCLUDE_FROM_ALL
    ${CMAKE_CURRENT_SOURCE_DIR}/Export.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/MemoryPool.cpp
)

target_include_directories(libkernel PRIVATE ${LIBS_INCLUDE_DIR})
