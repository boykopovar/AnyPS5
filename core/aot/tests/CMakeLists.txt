if(NOT APPLE AND NOT CMAKE_SYSTEM_PROCESSOR MATCHES "^(AMD64|amd64|x86_64)$")
    message(FATAL_ERROR "Leaf compiler execution tests require x86-64 or macOS with Rosetta")
endif()
find_package(Python3 REQUIRED COMPONENTS Interpreter)
if(COMMAND add_test_executable)
    add_test_executable(aot_ir_check TestAotCompiler.cpp)
else()
    add_executable(aot_ir_check TestAotCompiler.cpp)
endif()
target_link_libraries(aot_ir_check PRIVATE aot_compiler LLVMIRReader)
foreach(group IN ITEMS abi arithmetic moves shifts reject formats)
    add_test(NAME aot_leaf_${group}
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_aot_compiler.py
        --compiler $<TARGET_FILE:aot_compiler_cli> --host-compiler ${CMAKE_CXX_COMPILER}
        --ir-checker $<TARGET_FILE:aot_ir_check>
        --group ${group})
    set_tests_properties(aot_leaf_${group} PROPERTIES TIMEOUT 30)
endforeach()
