add_library(libkernel SHARED EXCLUDE_FROM_ALL
        ${CMAKE_CURRENT_SOURCE_DIR}/Export.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Equeue/Equeue.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/DirectMemory/Export.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/DirectMemory/DirectMemory.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/DirectMemory/MemoryPool.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Pthread/src/Mutex.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Pthread/src/Cond.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Pthread/src/Attr.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Pthread/src/Thread.cpp
)

target_include_directories(libkernel PRIVATE ${LIBS_INCLUDE_DIR})
target_link_libraries(libkernel PRIVATE libc)
if(WIN32)
    target_compile_definitions(libkernel PRIVATE _WIN32_WINNT=0x0602)
endif()
