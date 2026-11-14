add_library(aot_compiler STATIC
    src/ElfBinaryParser.cpp
    src/LlvmLifter.cpp
    src/AotCompilerDriver.cpp
)
target_include_directories(aot_compiler PUBLIC include)
target_include_directories(aot_compiler SYSTEM PUBLIC
    "${llvmSource}/include"
    "${CMAKE_CURRENT_BINARY_DIR}/../llvm/include"
)
target_compile_features(aot_compiler PUBLIC cxx_std_20)
target_link_libraries(aot_compiler PUBLIC LLVMCore LLVMObject LLVMX86CodeGen LLVMX86Desc LLVMX86Info)
add_executable(aot_compiler_cli src/Main.cpp)
target_link_libraries(aot_compiler_cli PRIVATE aot_compiler)
