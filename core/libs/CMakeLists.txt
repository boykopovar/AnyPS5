add_executable(nid_patcher
        ${CMAKE_CURRENT_SOURCE_DIR}/nid/src/Sha1.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/nid/src/NidCompute.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/nid/src/main.cpp
)
target_include_directories(nid_patcher PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/nid/include)
target_compile_features(nid_patcher PRIVATE cxx_std_20)

file(GLOB LIBS_DIRS LIST_DIRECTORIES true ${CMAKE_CURRENT_SOURCE_DIR}/prx/*)

set(PATCHED_STAMPS)

foreach(LIB_DIR ${LIBS_DIRS})
    if(IS_DIRECTORY ${LIB_DIR})
        get_filename_component(LIB_NAME ${LIB_DIR} NAME)

        add_library(${LIB_NAME} SHARED EXCLUDE_FROM_ALL ${LIB_DIR}/Export.cpp)
        target_include_directories(${LIB_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
        set_target_properties(${LIB_NAME} PROPERTIES
                PREFIX ""
                SUFFIX .prx
                RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/libs
                LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/libs
        )

        set(RPX_FILE ${CMAKE_CURRENT_BINARY_DIR}/libs/${LIB_NAME}.prx)
        set(STAMP_FILE ${CMAKE_CURRENT_BINARY_DIR}/libs/${LIB_NAME}.patched)

        if(${LIB_NAME} STREQUAL "libSceLibcInternal")
            target_link_options(${LIB_NAME} PRIVATE -nostartfiles -Wl,--allow-multiple-definition)
        endif()

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
