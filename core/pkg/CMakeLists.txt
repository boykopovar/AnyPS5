add_library(anyps5_pkg STATIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ContentContainer.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/InnerFileSystem.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/InnerMount.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/KrakenStream.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/NapsLayout.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/OodleLibrary.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/OuterImage.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Package.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/PackageFile.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/SelfImage.cpp
)
target_include_directories(anyps5_pkg PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
set_target_properties(anyps5_pkg PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_link_libraries(anyps5_pkg PUBLIC ${CMAKE_DL_LIBS})

if(BUILD_TESTING)
    add_test_executable(package_format_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/PackageFormatTests.cpp)
    target_link_libraries(package_format_tests PRIVATE anyps5_pkg)
    add_test(NAME package_format COMMAND package_format_tests)
endif()
