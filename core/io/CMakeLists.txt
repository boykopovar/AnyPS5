add_library(io STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/FileReader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/FileWriter.cpp
)
target_include_directories(io PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(io PRIVATE domain)
