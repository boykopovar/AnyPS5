add_library(elfpatcher STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ElfPatcher.cpp
)
target_include_directories(elfpatcher PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(elfpatcher PRIVATE domain)
