set(ANYPS5_RECOMPILER_DIR ${PROJECT_SOURCE_DIR}/core/shader/recompiler)
file(GLOB_RECURSE ANYPS5_TOOL_RECOMPILER_SOURCES CONFIGURE_DEPENDS ${ANYPS5_RECOMPILER_DIR}/*.cpp)
list(FILTER ANYPS5_TOOL_RECOMPILER_SOURCES EXCLUDE REGEX "^${ANYPS5_RECOMPILER_DIR}/(tests/|WgslBackend/tools/|Deserialize\\.cpp$|SpirvBackend/src/SpirvOptimizer\\.cpp$)")
set(ANYPS5_TOOL_RECOMPILER_VERSION ${CMAKE_CURRENT_BINARY_DIR}/ShaderCacheVersion.cpp)
file(CONFIGURE OUTPUT ${ANYPS5_TOOL_RECOMPILER_VERSION} CONTENT "#include \"ShaderCacheSourceVersion.hpp\"\n\nnamespace ShaderRecompiler::ShaderDiskCache::Generated {\n\nconstinit const std::uint64_t SourceVersion = 0;\nconstinit const std::span<const std::string_view> RecompilerSwitches = {};\n\n}\n")

add_library(aps5_tool_recompiler STATIC ${ANYPS5_TOOL_RECOMPILER_SOURCES} ${ANYPS5_TOOL_RECOMPILER_VERSION})
target_include_directories(aps5_tool_recompiler PUBLIC
        ${PROJECT_SOURCE_DIR}/core/libs
        ${ANYPS5_RECOMPILER_DIR}
        ${ANYPS5_RECOMPILER_DIR}/RdnaDecoder/include
        ${ANYPS5_RECOMPILER_DIR}/ControlFlow/include
        ${ANYPS5_RECOMPILER_DIR}/IntermediateRepresentation/include
        ${ANYPS5_RECOMPILER_DIR}/Translation/include
        ${ANYPS5_RECOMPILER_DIR}/Optimization/include
        ${ANYPS5_RECOMPILER_DIR}/SpirvBackend/include
        ${ANYPS5_RECOMPILER_DIR}/WgslBackend/include
        ${PROJECT_SOURCE_DIR}/3rdparty/SPIRV-Headers/include
)
target_compile_definitions(aps5_tool_recompiler PUBLIC ANYPS5_ENABLE_SPIRV_TOOLS=0)

add_executable(anyps5-wgsl WgslCompile.cpp)
target_link_libraries(anyps5-wgsl PRIVATE aps5_tool_recompiler)

if(BUILD_TESTING)
    add_executable(wgsl_native_compat_tests ${ANYPS5_RECOMPILER_DIR}/tests/WgslTarget.cpp)
    target_link_libraries(wgsl_native_compat_tests PRIVATE aps5_tool_recompiler)
    add_test(NAME wgsl_native_compat COMMAND wgsl_native_compat_tests)
endif()
