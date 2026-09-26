add_library(libkernel SHARED EXCLUDE_FROM_ALL
        ${CMAKE_CURRENT_SOURCE_DIR}/Equeue/Equeue.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Equeue/src/EventAccessors.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/File/src/NativeStat.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/File/src/Open.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/File/src/Stdio.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Semaphore/src/Semaphore.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/DirectMemory/Export.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/DirectMemory/DirectMemory.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/DirectMemory/PosixMemory.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/DirectMemory/MemoryPool.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/DirectMemory/MemoryPoolExport.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Pthread/src/Mutex.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Pthread/src/Cond.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Pthread/src/Attr.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Pthread/src/Thread.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Pthread/src/Tsd.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Pthread/src/Rwlock.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Pthread/src/Sem.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Pthread/Posix/Attr.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Pthread/Posix/Cond.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Pthread/Posix/Mutex.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Pthread/Posix/Rwlock.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Pthread/Posix/Thread.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Pthread/Posix/Once.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Pthread/Posix/Tsd.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Pthread/Posix/Sem.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Socket/src/Socket.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Socket/src/Udp.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Socket/src/AddressConversion.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/EventFlag/src/EventFlag.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Module/src/Module.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Module/src/DynamicLoader.cpp
        ${LIBS_INCLUDE_DIR}/nid/src/NidCompute.cpp
        ${LIBS_INCLUDE_DIR}/nid/src/Sha1.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Aio/src/Aio.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/System/src/Coredump.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/System/src/Exception.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/System/src/Sanitizer.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/System/src/Rtld.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/System/src/Process.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/System/src/SystemConfiguration.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/System/src/Signals.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Time/Time.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/AppMetadata/src/AppMetadata.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/AppMetadata/src/ParamJsonParser.cpp
)

target_include_directories(libkernel PRIVATE ${LIBS_INCLUDE_DIR})
target_include_directories(libkernel PRIVATE ${LIBS_INCLUDE_DIR}/nid/include)
target_link_libraries(libkernel PRIVATE ${CMAKE_DL_LIBS})
target_link_libraries(libkernel PRIVATE libc)
if(WIN32)
    target_link_libraries(libkernel PRIVATE ws2_32)
    target_compile_definitions(libkernel PRIVATE _WIN32_WINNT=0x0602)
endif()
configure_windows_unwind(libkernel)
