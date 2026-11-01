include(ExternalProject)

get_filename_component(curlRepositoryRoot "${CMAKE_CURRENT_SOURCE_DIR}/../../../.." ABSOLUTE)
set(curlSourceDir ${curlRepositoryRoot}/3rdparty/curl)
set(curlBackendDir ${CMAKE_CURRENT_BINARY_DIR}/backend)
set(curlBackendArchive ${curlBackendDir}/lib/libcurl.a)

if(NOT TARGET mbedtls OR NOT TARGET mbedx509 OR NOT TARGET mbedcrypto OR NOT TARGET nghttp2_static)
    message(FATAL_ERROR "libcurl requires the bundled libSceSsl and libSceHttp2 backend targets")
endif()

get_target_property(curlMbedDefinitions mbedtls INTERFACE_COMPILE_DEFINITIONS)
if(NOT curlMbedDefinitions)
    set(curlMbedDefinitions "")
endif()
get_target_property(curlNgBinaryDir nghttp2_static BINARY_DIR)
file(GENERATE OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/BackendOptions.cmake CONTENT
    "add_compile_definitions([=[${curlMbedDefinitions}]=] MBEDTLS_ALLOW_PRIVATE_ACCESS NGHTTP2_STATICLIB curl_strequal=host_curl_strequal curl_strnequal=host_curl_strnequal)\ninclude_directories([=[${curlNgBinaryDir}/includes]=])\nset(CMAKE_POSITION_INDEPENDENT_CODE ON)\n")

set(curlBackendOptions
    -DCMAKE_C_COMPILER:FILEPATH=${CMAKE_C_COMPILER}
    -DCMAKE_MAKE_PROGRAM:FILEPATH=${CMAKE_MAKE_PROGRAM}
    -DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}
    -DCMAKE_DEBUG_POSTFIX:STRING=
    -DCMAKE_PROJECT_CURL_INCLUDE:FILEPATH=${CMAKE_CURRENT_BINARY_DIR}/BackendOptions.cmake
    -DCMAKE_POLICY_VERSION_MINIMUM:STRING=3.5
    -DBUILD_SHARED_LIBS:BOOL=OFF
    -DBUILD_CURL_EXE:BOOL=OFF
    -DBUILD_TESTING:BOOL=OFF
    -DCURL_ENABLE_EXPORT_TARGET:BOOL=OFF
    -DCURL_USE_MBEDTLS:BOOL=ON
    -DMBEDTLS_INCLUDE_DIRS:PATH=${curlRepositoryRoot}/3rdparty/mbedtls/include
    -DMBEDTLS_LIBRARY:FILEPATH=$<TARGET_FILE:mbedtls>
    -DMBEDX509_LIBRARY:FILEPATH=$<TARGET_FILE:mbedx509>
    -DMBEDCRYPTO_LIBRARY:FILEPATH=$<TARGET_FILE:mbedcrypto>
    -DUSE_NGHTTP2:BOOL=ON
    -DNGHTTP2_INCLUDE_DIR:PATH=${curlRepositoryRoot}/3rdparty/nghttp2/lib/includes
    -DNGHTTP2_LIBRARY:FILEPATH=$<TARGET_FILE:nghttp2_static>
    -DCMAKE_DISABLE_FIND_PACKAGE_Perl:BOOL=ON
    -DCMAKE_DISABLE_FIND_PACKAGE_ZLIB:BOOL=ON
    -DCURL_ZLIB:BOOL=OFF
    -DPICKY_COMPILER:BOOL=OFF
)
foreach(option CURL_USE_OPENSSL CURL_USE_SCHANNEL CURL_USE_SECTRANSP CURL_USE_BEARSSL CURL_USE_NSS CURL_USE_WOLFSSL
        CURL_USE_LIBSSH CURL_USE_LIBSSH2 CURL_USE_GSSAPI CURL_WINDOWS_SSPI USE_LIBIDN2 USE_WIN32_IDN
        CURL_BROTLI CURL_ZSTD ENABLE_ARES USE_NGTCP2 USE_QUICHE)
    list(APPEND curlBackendOptions -D${option}:BOOL=OFF)
endforeach()
foreach(protocol DICT FTP GOPHER IMAP LDAP LDAPS MQTT POP3 RTSP SMB SMTP TELNET TFTP)
    list(APPEND curlBackendOptions -DCURL_DISABLE_${protocol}:BOOL=ON)
endforeach()
if(CMAKE_TOOLCHAIN_FILE)
    list(APPEND curlBackendOptions -DCMAKE_TOOLCHAIN_FILE:FILEPATH=${CMAKE_TOOLCHAIN_FILE})
endif()

ExternalProject_Add(anyps5_curl_build
    SOURCE_DIR ${curlSourceDir}
    BINARY_DIR ${curlBackendDir}
    DOWNLOAD_COMMAND ""
    UPDATE_COMMAND ""
    PATCH_COMMAND ""
    INSTALL_COMMAND ""
    CMAKE_ARGS ${curlBackendOptions}
    BUILD_ALWAYS TRUE
    BUILD_BYPRODUCTS ${curlBackendArchive}
    DEPENDS mbedtls mbedx509 mbedcrypto nghttp2_static
    EXCLUDE_FROM_ALL TRUE
)
add_library(anyps5_curl_backend STATIC IMPORTED)
set_target_properties(anyps5_curl_backend PROPERTIES
    IMPORTED_LOCATION ${curlBackendArchive}
    INTERFACE_LINK_LIBRARIES "mbedtls;mbedx509;mbedcrypto;nghttp2_static;${CMAKE_DL_LIBS}"
)
add_dependencies(anyps5_curl_backend anyps5_curl_build)
if(WIN32)
    set_property(TARGET anyps5_curl_backend APPEND PROPERTY INTERFACE_LINK_LIBRARIES ws2_32 winmm crypt32 bcrypt)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set_property(TARGET anyps5_curl_backend APPEND PROPERTY INTERFACE_LINK_LIBRARIES pthread)
endif()

add_library(libcurl SHARED EXCLUDE_FROM_ALL Export.cpp)
target_include_directories(libcurl PUBLIC ${curlSourceDir}/include PRIVATE ${LIBS_INCLUDE_DIR})
target_link_libraries(libcurl PRIVATE libc libkernel anyps5_curl_backend)
if(MINGW OR CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_options(libcurl PRIVATE LINKER:--exclude-libs,ALL)
endif()
configure_windows_unwind(libcurl)
install(FILES ${curlSourceDir}/COPYING DESTINATION licenses/curl)

if(BUILD_TESTING)
    foreach(test_topic IN ITEMS Utility Url SlistMime Options Handles Transfers)
        set(test_name guest_curl_${test_topic}_tests)
        string(TOLOWER "${test_topic}" test_slug)
        add_test_executable(${test_name} tests/Curl${test_topic}.cpp)
        target_include_directories(${test_name} PRIVATE ${LIBS_INCLUDE_DIR})
        target_link_libraries(${test_name} PRIVATE libcurl libkernel libc)
        if(test_topic STREQUAL "Transfers")
            target_link_libraries(${test_name} PRIVATE anyps5_curl_backend)
        endif()
        configure_windows_unwind(${test_name})
        add_test(NAME guest_curl_${test_slug} COMMAND ${test_name})
        set_tests_properties(guest_curl_${test_slug} PROPERTIES TIMEOUT 30)
        if(WIN32)
            set_tests_properties(guest_curl_${test_slug} PROPERTIES ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:libcurl>")
        endif()
    endforeach()
endif()
