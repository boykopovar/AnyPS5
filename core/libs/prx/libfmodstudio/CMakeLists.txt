add_library(libfmodstudio SHARED EXCLUDE_FROM_ALL
        ${CMAKE_CURRENT_SOURCE_DIR}/Export.cpp
)

target_include_directories(libfmodstudio PRIVATE
        ${LIBS_INCLUDE_DIR}
)
target_link_libraries(libfmodstudio PRIVATE libkernel libc)
target_compile_options(libfmodstudio PRIVATE -fkeep-inline-functions)
configure_windows_unwind(libfmodstudio)
