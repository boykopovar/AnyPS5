set(RELINKER_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)

set(RELINKER_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/parsing/ElfReader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/parsing/SceDynlibParser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/parsing/SdkRevisionProfile.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/analysis/SyscallScanner.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/analysis/CallSiteResolver.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/analysis/ValidationPolicy.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/output/SysVDynamicSectionBuilder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/output/CallRegistryWriter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/pipeline/RelinkerPipeline.cpp
)

add_executable(relinker ${RELINKER_SOURCES})
target_include_directories(relinker PRIVATE ${RELINKER_INCLUDE_DIR})
target_link_libraries(relinker PRIVATE domain elfpatcher io)
