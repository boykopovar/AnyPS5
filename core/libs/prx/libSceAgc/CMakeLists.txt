add_library(libSceAgc SHARED EXCLUDE_FROM_ALL
        ${CMAKE_CURRENT_SOURCE_DIR}/Export.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Shader/Export.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Shader/ShaderUtils.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Acb/Export.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Cb/Export.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/DcbDraw/Export.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/DcbState/Export.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/DcbFlow/Export.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Patch/Export.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Predication/Export.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Misc/Export.cpp
)

target_include_directories(libSceAgc PRIVATE ${LIBS_INCLUDE_DIR})
add_shader_recompiler(shader_recompiler_agc libc)
target_link_libraries(libSceAgc PRIVATE shader_recompiler_agc libc)
