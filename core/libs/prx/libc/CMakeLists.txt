target_sources(libc PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Export.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/General.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/specifics/gcc/StackGuard.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/specifics/linux/DlIteratePhdr.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Strings.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/StringExtensions.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Math.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Time.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/FileAndHeap.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/StandardStreams.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/GuestAllocations.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/GuestMemoryTracking.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/GuestMemoryBacking.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/GuestHeapStorage.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/GuestHeap.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/HeapDiagnostics.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Mspace.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ApplicationHeap.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ApplicationProcessParameters.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/JumpBuffer.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Formatting.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/RuntimeSupport.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/LocaleSupport.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Process.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Environment.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Directory.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Filesystem.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Errors.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/CxxAbiSupport.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/exception/Exports.cpp
)

if(WIN32)
    target_sources(libc PRIVATE src/MemoryTrackingWindows.cpp src/MemoryBackingWindows.cpp)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|amd64)$")
    target_sources(libc PRIVATE src/MemoryTrackingLinux.cpp src/MemoryBackingLinux.cpp)
else()
    message(FATAL_ERROR "Guest GPU memory tracking requires Windows or x86-64 Linux")
endif()

target_include_directories(libc PRIVATE
        ${LIBS_INCLUDE_DIR}
)
