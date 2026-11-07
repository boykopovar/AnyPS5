add_executable(ps5lift
        src/main.cpp
        src/GuestImage.cpp
        src/Discovery.cpp
        src/FunctionLifter.cpp
        src/ModuleLifter.cpp
        src/TableLifter.cpp
        src/instructions/Control.cpp
        src/instructions/Crypto.cpp
        src/instructions/Dispatch.cpp
        src/instructions/Float.cpp
        src/instructions/Integer.cpp
        src/instructions/Shuffle.cpp
        src/instructions/String.cpp
        src/instructions/System.cpp
        src/instructions/Vector.cpp
        src/instructions/X87.cpp
        src/Codegen.cpp
        src/Target.cpp
        src/Compile.cpp
        src/Program.cpp)
target_include_directories(ps5lift PRIVATE include ${LLVM_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR}/runtime/include)
target_compile_definitions(ps5lift PRIVATE ${LLVM_DEFINITIONS})
if(LLVM_LINK_LLVM_DYLIB)
    set(PS5LIFT_LLVM_LIBS LLVM)
else()
    llvm_map_components_to_libnames(PS5LIFT_LLVM_LIBS core support passes target mc object
            aarch64codegen aarch64asmparser aarch64desc aarch64info
            x86codegen x86asmparser x86desc x86info)
endif()
target_link_libraries(ps5lift PRIVATE anyps5_elf Zydis ${PS5LIFT_LLVM_LIBS})
