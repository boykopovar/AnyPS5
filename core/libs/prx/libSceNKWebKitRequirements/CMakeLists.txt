include(${CMAKE_SOURCE_DIR}/cmake/LibXml2.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/LibPsl.cmake)

add_library(libSceNKWebKitRequirements SHARED EXCLUDE_FROM_ALL Export.cpp Zlib.cpp Jpeg.cpp Xml.cpp Psl.cpp)
target_include_directories(libSceNKWebKitRequirements PRIVATE ${LIBS_INCLUDE_DIR}
    ${CMAKE_SOURCE_DIR}/3rdparty/libjpeg-turbo/src/spng/zlib
    ${CMAKE_SOURCE_DIR}/3rdparty/libjpeg-turbo/src
    ${CMAKE_BINARY_DIR}/libjpeg-turbo/include
    ${CMAKE_SOURCE_DIR}/3rdparty/libxml2/include
    ${CMAKE_BINARY_DIR}/3rdparty/libxml2
    ${CMAKE_SOURCE_DIR}/3rdparty/libpsl/include
    ${CMAKE_BINARY_DIR}/3rdparty/libpsl)
set_target_properties(libSceNKWebKitRequirements PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(libSceNKWebKitRequirements)
target_link_libraries(libSceNKWebKitRequirements PRIVATE jpeg_turbo LibXml2 psl)
if(MINGW OR CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_options(libSceNKWebKitRequirements PRIVATE LINKER:--exclude-libs,ALL)
endif()

if(BUILD_TESTING)
    add_test_executable(guest_webkit_zlib_tests ${LIBS_INCLUDE_DIR}/tests/GuestWebKitZlib.cpp)
    target_include_directories(guest_webkit_zlib_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_webkit_zlib_tests PRIVATE libSceNKWebKitRequirements libc)
    configure_windows_unwind(guest_webkit_zlib_tests)
    add_test(NAME guest_webkit_zlib COMMAND guest_webkit_zlib_tests)
    set_tests_properties(guest_webkit_zlib PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(guest_webkit_zlib PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceNKWebKitRequirements>")
    endif()
    foreach(topic IN ITEMS Jpeg Xml Psl)
        string(TOLOWER ${topic} test_name)
        add_test_executable(guest_nk_req_${test_name}_tests tests/${topic}.cpp)
        target_include_directories(guest_nk_req_${test_name}_tests PRIVATE
            ${LIBS_INCLUDE_DIR}
            ${CMAKE_SOURCE_DIR}/3rdparty/libjpeg-turbo/src
            ${CMAKE_BINARY_DIR}/libjpeg-turbo/include
            ${CMAKE_SOURCE_DIR}/3rdparty/libxml2/include
            ${CMAKE_BINARY_DIR}/3rdparty/libxml2
            ${CMAKE_SOURCE_DIR}/3rdparty/libpsl/include
            ${CMAKE_BINARY_DIR}/3rdparty/libpsl)
        target_link_libraries(guest_nk_req_${test_name}_tests PRIVATE libSceNKWebKitRequirements jpeg_turbo LibXml2 psl libc)
        configure_windows_unwind(guest_nk_req_${test_name}_tests)
        add_test(NAME guest_nk_req_${test_name} COMMAND guest_nk_req_${test_name}_tests)
        set_tests_properties(guest_nk_req_${test_name} PROPERTIES TIMEOUT 30)
        if(WIN32)
            set_tests_properties(guest_nk_req_${test_name} PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceNKWebKitRequirements>")
        endif()
    endforeach()
endif()
