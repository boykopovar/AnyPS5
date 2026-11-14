cmake_minimum_required(VERSION 3.20)
project(AnyPS5NativeShaderSnapshot CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
get_filename_component(fixtureRoot "${CMAKE_CURRENT_SOURCE_DIR}/../../../../.." ABSOLUTE)
set(ANYPS5_NATIVE_SOURCE "${fixtureRoot}" CACHE PATH "Source tree whose native shader output is checked")
set(recompiler "${ANYPS5_NATIVE_SOURCE}/core/shader/recompiler")
file(GLOB_RECURSE sources CONFIGURE_DEPENDS "${recompiler}/*.cpp")
list(FILTER sources EXCLUDE REGEX "/tests/|/WgslBackend/tools/|/Deserialize\\.cpp$|/SpirvBackend/src/SpirvOptimizer\\.cpp$")
file(CONFIGURE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/ShaderCacheVersion.cpp" CONTENT "#include \"ShaderCacheSourceVersion.hpp\"\nnamespace ShaderRecompiler::ShaderDiskCache::Generated { constinit const std::uint64_t SourceVersion = 0; constinit const std::span<const std::string_view> RecompilerSwitches = {}; }\n")
add_library(snapshot_recompiler STATIC ${sources} "${CMAKE_CURRENT_BINARY_DIR}/ShaderCacheVersion.cpp")
target_compile_definitions(snapshot_recompiler PUBLIC ANYPS5_ENABLE_SPIRV_TOOLS=0)
target_include_directories(snapshot_recompiler PUBLIC
        "${ANYPS5_NATIVE_SOURCE}/core/libs"
        "${recompiler}"
        "${recompiler}/RdnaDecoder/include"
        "${recompiler}/ControlFlow/include"
        "${recompiler}/IntermediateRepresentation/include"
        "${recompiler}/Translation/include"
        "${recompiler}/Optimization/include"
        "${recompiler}/SpirvBackend/include"
        "${recompiler}/WgslBackend/include"
        "${fixtureRoot}/3rdparty/SPIRV-Headers/include"
)
add_executable(shader_native_snapshot "${CMAKE_CURRENT_SOURCE_DIR}/../NativeSnapshot.cpp")
target_link_libraries(shader_native_snapshot PRIVATE snapshot_recompiler)
enable_testing()
add_test(NAME shader_native_snapshot COMMAND shader_native_snapshot "${CMAKE_CURRENT_BINARY_DIR}/native-snapshots")
set_tests_properties(shader_native_snapshot PROPERTIES TIMEOUT 30)
