set(RELINKER_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)


set(RELINKER_HEADERS
    ${RELINKER_INCLUDE_DIR}/relinker/Types.hpp
    ${RELINKER_INCLUDE_DIR}/relinker/IValidationPolicy.hpp
    ${RELINKER_INCLUDE_DIR}/relinker/IElfReader.hpp
    ${RELINKER_INCLUDE_DIR}/relinker/ISdkRevisionProfile.hpp
    ${RELINKER_INCLUDE_DIR}/relinker/ISceDynlibParser.hpp
    ${RELINKER_INCLUDE_DIR}/relinker/ISyscallScanner.hpp
    ${RELINKER_INCLUDE_DIR}/relinker/ICallSiteResolver.hpp
    ${RELINKER_INCLUDE_DIR}/relinker/ISysVDynamicSectionBuilder.hpp
    ${RELINKER_INCLUDE_DIR}/relinker/ICallRegistryWriter.hpp
)

set(RELINKER_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ValidationPolicy.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ElfReader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/SdkRevisionProfile.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/SceDynlibParser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/SyscallScanner.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/CallSiteResolver.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/SysVDynamicSectionBuilder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/CallRegistryWriter.cpp
)

add_executable(relinker ${RELINKER_SOURCES} ${RELINKER_HEADERS})
target_include_directories(relinker PRIVATE ${RELINKER_INCLUDE_DIR})
