add_executable(nid_patcher
        ${CMAKE_CURRENT_SOURCE_DIR}/nid/src/Sha1.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/nid/src/NidCompute.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/nid/src/ElfNidPatcher.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/nid/src/PeNidPatcher.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/nid/src/BinaryPatcherFactory.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/nid/src/main.cpp
)
target_include_directories(nid_patcher PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/nid/include)
target_compile_features(nid_patcher PRIVATE cxx_std_20)

set(LIBC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/prx/libc)
add_library(libc SHARED EXCLUDE_FROM_ALL ${LIBC_DIR}/Export.cpp)
target_include_directories(libc PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_options(libc PRIVATE -static-libgcc -static-libstdc++)
set_target_properties(libc PROPERTIES
        PREFIX ""
        SUFFIX .prx
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/libs
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/libs
)

set(LIBC_PRX_FILE ${CMAKE_CURRENT_BINARY_DIR}/libs/libc.prx)
set(LIBC_STAMP_FILE ${CMAKE_CURRENT_BINARY_DIR}/libs/libc.patched)

add_custom_command(
        OUTPUT ${LIBC_STAMP_FILE}
        COMMAND $<TARGET_FILE:nid_patcher> libc ${LIBC_PRX_FILE}
        COMMAND ${CMAKE_COMMAND} -E touch ${LIBC_STAMP_FILE}
        DEPENDS nid_patcher libc
        COMMENT "Patching NIDs for libc"
)

file(GLOB LIBS_DIRS LIST_DIRECTORIES true ${CMAKE_CURRENT_SOURCE_DIR}/prx/*)
list(REMOVE_ITEM LIBS_DIRS ${LIBC_DIR})

set(PATCHED_STAMPS ${LIBC_STAMP_FILE})

foreach(LIB_DIR ${LIBS_DIRS})
    if(IS_DIRECTORY ${LIB_DIR})
        get_filename_component(LIB_NAME ${LIB_DIR} NAME)

        add_library(${LIB_NAME} SHARED EXCLUDE_FROM_ALL ${LIB_DIR}/Export.cpp)
        target_include_directories(${LIB_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
        target_link_libraries(${LIB_NAME} PRIVATE libc)
        set_target_properties(${LIB_NAME} PROPERTIES
                PREFIX ""
                SUFFIX .prx
                RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/libs
                LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/libs
        )

        set(RPX_FILE ${CMAKE_CURRENT_BINARY_DIR}/libs/${LIB_NAME}.prx)
        set(STAMP_FILE ${CMAKE_CURRENT_BINARY_DIR}/libs/${LIB_NAME}.patched)

        add_custom_command(
                OUTPUT ${STAMP_FILE}
                COMMAND $<TARGET_FILE:nid_patcher> ${LIB_NAME} ${RPX_FILE}
                COMMAND ${CMAKE_COMMAND} -E touch ${STAMP_FILE}
                DEPENDS nid_patcher ${LIB_NAME}
                COMMENT "Patching NIDs for ${LIB_NAME}"
        )

        list(APPEND PATCHED_STAMPS ${STAMP_FILE})
    endif()
endforeach()

add_custom_target(libs DEPENDS ${PATCHED_STAMPS})
