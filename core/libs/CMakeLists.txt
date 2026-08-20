add_executable(nid_patcher EXCLUDE_FROM_ALL
    ${CMAKE_CURRENT_SOURCE_DIR}/nid/src/Sha1.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/nid/src/NidCompute.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/nid/src/main.cpp
)
target_include_directories(nid_patcher PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/nid/include)
target_compile_features(nid_patcher PRIVATE cxx_std_20)

file(GLOB LIBS_DIRS LIST_DIRECTORIES true ${CMAKE_CURRENT_SOURCE_DIR}/lib*)

set(LIB_TARGETS)

foreach(LIB_DIR ${LIBS_DIRS})
    if(IS_DIRECTORY ${LIB_DIR})
        get_filename_component(LIB_NAME ${LIB_DIR} NAME)

        add_library(${LIB_NAME} SHARED EXCLUDE_FROM_ALL ${LIB_DIR}/Export.cpp)
        set_target_properties(${LIB_NAME} PROPERTIES
                PREFIX ""
                SUFFIX .rpx
                RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/libs
                LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/libs
        )

        add_dependencies(${LIB_NAME} nid_patcher)

        add_custom_command(TARGET ${LIB_NAME} POST_BUILD
            COMMAND $<TARGET_FILE:nid_patcher> ${LIB_NAME} $<TARGET_FILE:${LIB_NAME}>
            COMMENT "Patching NIDs"
        )

        list(APPEND LIB_TARGETS ${LIB_NAME})
    endif()
endforeach()

add_custom_target(libs DEPENDS nid_patcher ${LIB_TARGETS})
