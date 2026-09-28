add_library(libfmod SHARED EXCLUDE_FROM_ALL
        ${CMAKE_CURRENT_SOURCE_DIR}/Export.cpp
)

target_include_directories(libfmod PRIVATE
        ${LIBS_INCLUDE_DIR}
        ${CMAKE_SOURCE_DIR}/3rdparty/SDL2/include
)
target_link_libraries(libfmod PRIVATE libkernel libc SDL2-static)
target_compile_options(libfmod PRIVATE -fkeep-inline-functions)
configure_windows_unwind(libfmod)
