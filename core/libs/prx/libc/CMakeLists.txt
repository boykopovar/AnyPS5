target_sources(libc PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Export.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/General.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/specifics/gcc/StackGuard.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/specifics/windows/CrashReport.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/specifics/linux/DlIteratePhdr.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Strings.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Math.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Time.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/FileAndHeap.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/StdioExtras.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/GuestAllocations.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/GuestArena.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/GuestHeap.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ApplicationHeap.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ApplicationProcessParameters.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/JumpBuffer.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Formatting.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/RuntimeSupport.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/LocaleSupport.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Process.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/CxxAbiSupport.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/exception/Exports.cpp
)

target_include_directories(libc PRIVATE
        ${LIBS_INCLUDE_DIR}
)
