add_executable(relinker_wasm Bindings.cpp)
target_link_libraries(relinker_wasm PRIVATE relinker_core)
target_compile_options(relinker_wasm PRIVATE -fexceptions)
target_link_options(relinker_wasm PRIVATE
    --bind
    --no-entry
    -fexceptions
    -sMODULARIZE=1
    -sEXPORT_ES6=1
    -sEXPORT_NAME=createRelinker
    -sALLOW_MEMORY_GROWTH=1
    -sMAXIMUM_MEMORY=2147483648
    -sSTACK_SIZE=1048576
    -sFILESYSTEM=0
    -sENVIRONMENT=web,worker,node
)
set_target_properties(relinker_wasm PROPERTIES
    SUFFIX ".mjs"
    CXX_EXTENSIONS OFF
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/web/native"
)

find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_custom_target(relinker_web
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/build_web.py" --build "${CMAKE_BINARY_DIR}"
    DEPENDS relinker_wasm
    VERBATIM
)
