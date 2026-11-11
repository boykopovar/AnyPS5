set(ZLIB_BUILD_TESTING OFF)
set(ZLIB_BUILD_SHARED OFF)
set(ZLIB_BUILD_STATIC ON)
set(ZLIB_INSTALL OFF)
add_subdirectory(${CMAKE_SOURCE_DIR}/3rdparty/zlib ${CMAKE_BINARY_DIR}/3rdparty/zlib EXCLUDE_FROM_ALL)
set_target_properties(zlibstatic PROPERTIES POSITION_INDEPENDENT_CODE ON C_VISIBILITY_PRESET hidden)

include(${CMAKE_SOURCE_DIR}/cmake/LibXml2.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/LibPsl.cmake)

add_library(libSceNKWebKitRequirements SHARED EXCLUDE_FROM_ALL Export.cpp Zlib.cpp src/Sqlite.cpp Jpeg.cpp Xml.cpp Psl.cpp)
target_include_directories(libSceNKWebKitRequirements PRIVATE ${LIBS_INCLUDE_DIR}
    ${CMAKE_SOURCE_DIR}/3rdparty/libjpeg-turbo/src
    ${CMAKE_BINARY_DIR}/libjpeg-turbo/include
    ${CMAKE_SOURCE_DIR}/3rdparty/libxml2/include
    ${CMAKE_BINARY_DIR}/3rdparty/libxml2
    ${CMAKE_SOURCE_DIR}/3rdparty/libpsl/include
    ${CMAKE_BINARY_DIR}/3rdparty/libpsl)
set_target_properties(libSceNKWebKitRequirements PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(libSceNKWebKitRequirements)
target_link_libraries(libSceNKWebKitRequirements PRIVATE sqlite3 zlibstatic jpeg_turbo LibXml2 psl)
if(MINGW OR CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_options(libSceNKWebKitRequirements PRIVATE LINKER:--exclude-libs,ALL)
endif()

if(BUILD_TESTING)
    add_test_executable(guest_nk_req_sqlite_tests ${LIBS_INCLUDE_DIR}/tests/GuestNKWebKitSqlite.cpp)
    target_include_directories(guest_nk_req_sqlite_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_nk_req_sqlite_tests PRIVATE libSceNKWebKitRequirements libc)
    configure_windows_unwind(guest_nk_req_sqlite_tests)
    add_test(NAME guest_nk_req_sqlite COMMAND guest_nk_req_sqlite_tests)
    set_tests_properties(guest_nk_req_sqlite PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(guest_nk_req_sqlite PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceNKWebKitRequirements>")
    endif()

    add_test_executable(guest_nk_req_sqlite_binding_tests tests/SqliteBindingDestructors.cpp src/Sqlite.cpp)
    target_include_directories(guest_nk_req_sqlite_binding_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_nk_req_sqlite_binding_tests PRIVATE sqlite3 libc)
    configure_windows_unwind(guest_nk_req_sqlite_binding_tests)
    foreach(mode IN ITEMS lifetime errors conversion threads)
        add_test(NAME guest_nk_req_sqlite_binding_${mode} COMMAND guest_nk_req_sqlite_binding_tests ${mode})
        set_tests_properties(guest_nk_req_sqlite_binding_${mode} PROPERTIES TIMEOUT 30)
        if(WIN32)
            set_tests_properties(guest_nk_req_sqlite_binding_${mode} PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
        endif()
    endforeach()

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

    add_test_executable(guest_webkit_gzip_tests ${LIBS_INCLUDE_DIR}/tests/GuestWebkitGzip.cpp)
    target_include_directories(guest_webkit_gzip_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_webkit_gzip_tests PRIVATE libSceNKWebKitRequirements zlibstatic)
    configure_windows_unwind(guest_webkit_gzip_tests)
    add_test(NAME guest_webkit_gzip COMMAND guest_webkit_gzip_tests)
    add_test(NAME guest_webkit_gzip_large_totals COMMAND guest_webkit_gzip_tests --large-totals)
    add_test(NAME guest_webkit_gzip_large_trailer COMMAND guest_webkit_gzip_tests --large-gzip)
    set_tests_properties(guest_webkit_gzip guest_webkit_gzip_large_totals guest_webkit_gzip_large_trailer PROPERTIES TIMEOUT 90)
    if(WIN32)
        set_tests_properties(guest_webkit_gzip guest_webkit_gzip_large_totals guest_webkit_gzip_large_trailer PROPERTIES
            ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceNKWebKitRequirements>")
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

if(BUILD_TESTING)
    add_test_executable(guest_ubsan_cfi_tests ${LIBS_INCLUDE_DIR}/tests/GuestUbsanCfi.cpp)
    target_include_directories(guest_ubsan_cfi_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_ubsan_cfi_tests PRIVATE libSceNKWebKitRequirements libkernel_sys libc)
    configure_windows_unwind(guest_ubsan_cfi_tests)
    add_test(NAME guest_ubsan_cfi COMMAND guest_ubsan_cfi_tests)
    set_tests_properties(guest_ubsan_cfi PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(guest_ubsan_cfi PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceNKWebKitRequirements>\;$<TARGET_FILE_DIR:libkernel_sys>")
    endif()
endif()
