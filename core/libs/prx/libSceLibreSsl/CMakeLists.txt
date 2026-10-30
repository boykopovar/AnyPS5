add_library(libSceLibreSsl SHARED EXCLUDE_FROM_ALL ${CMAKE_CURRENT_SOURCE_DIR}/Export.cpp ${CMAKE_CURRENT_SOURCE_DIR}/src/Digest.cpp ${CMAKE_CURRENT_SOURCE_DIR}/src/Runtime.cpp ${CMAKE_CURRENT_SOURCE_DIR}/src/Aes.cpp ${CMAKE_CURRENT_SOURCE_DIR}/src/Error.cpp ${CMAKE_CURRENT_SOURCE_DIR}/src/Ocsp.cpp ${CMAKE_CURRENT_SOURCE_DIR}/src/Stack.cpp ${CMAKE_CURRENT_SOURCE_DIR}/src/Ui.cpp ${CMAKE_CURRENT_SOURCE_DIR}/src/Rsa.cpp ${CMAKE_CURRENT_SOURCE_DIR}/src/X509StoreContext.cpp ${CMAKE_CURRENT_SOURCE_DIR}/src/Pkey.cpp)
target_include_directories(libSceLibreSsl PRIVATE ${LIBS_INCLUDE_DIR})
target_link_libraries(libSceLibreSsl PRIVATE mbedcrypto)
configure_windows_unwind(libSceLibreSsl)
target_sources(libSceLibreSsl PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/Asn1.cpp ${CMAKE_CURRENT_SOURCE_DIR}/src/ExData.cpp)

if(BUILD_TESTING)
    add_test_executable(guest_libressl_x509_errors_tests ${LIBS_INCLUDE_DIR}/tests/GuestLibreSslX509Errors.cpp)
    target_include_directories(guest_libressl_x509_errors_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_libressl_x509_errors_tests PRIVATE libSceLibreSsl libc)
    configure_windows_unwind(guest_libressl_x509_errors_tests)
    add_test(NAME guest_libressl_x509_errors COMMAND guest_libressl_x509_errors_tests)
    set_tests_properties(guest_libressl_x509_errors PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(guest_libressl_x509_errors PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceLibreSsl>")
    endif()

    add_test_executable(guest_libressl_digest_tests ${LIBS_INCLUDE_DIR}/tests/GuestLibreSslDigest.cpp)
    target_include_directories(guest_libressl_digest_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_libressl_digest_tests PRIVATE libSceLibreSsl libc)
    configure_windows_unwind(guest_libressl_digest_tests)
    add_test(NAME guest_libressl_digest COMMAND guest_libressl_digest_tests)
    set_tests_properties(guest_libressl_digest PROPERTIES TIMEOUT 30)
    add_test_executable(guest_libressl_asn1_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/Asn1.cpp)
    target_include_directories(guest_libressl_asn1_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_libressl_asn1_tests PRIVATE libSceLibreSsl libc)
    configure_windows_unwind(guest_libressl_asn1_tests)
    add_test(NAME guest_libressl_asn1 COMMAND guest_libressl_asn1_tests)
    add_test(NAME guest_libressl_asn1_allocator COMMAND guest_libressl_asn1_tests allocator)
    set_tests_properties(guest_libressl_asn1 guest_libressl_asn1_allocator PROPERTIES TIMEOUT 30)
    add_test_executable(guest_libressl_ex_data_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/ExData.cpp)
    target_include_directories(guest_libressl_ex_data_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_libressl_ex_data_tests PRIVATE libSceLibreSsl libc)
    configure_windows_unwind(guest_libressl_ex_data_tests)
    add_test(NAME guest_libressl_ex_data COMMAND guest_libressl_ex_data_tests)
    set_tests_properties(guest_libressl_ex_data PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(guest_libressl_digest guest_libressl_asn1 guest_libressl_asn1_allocator guest_libressl_ex_data PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceLibreSsl>")
    endif()

    add_test_executable(guest_libressl_des_tests ${LIBS_INCLUDE_DIR}/tests/GuestLibreSslDes.cpp)
    target_include_directories(guest_libressl_des_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_libressl_des_tests PRIVATE libSceLibreSsl libc)
    configure_windows_unwind(guest_libressl_des_tests)
    add_test(NAME guest_libressl_des COMMAND guest_libressl_des_tests)
    set_tests_properties(guest_libressl_des PROPERTIES TIMEOUT 20)
    if(WIN32)
        set_tests_properties(guest_libressl_des PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceLibreSsl>")
    endif()

    add_test_executable(guest_libressl_runtime_tests ${LIBS_INCLUDE_DIR}/tests/GuestLibreSslRuntime.cpp)
    target_include_directories(guest_libressl_runtime_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_libressl_runtime_tests PRIVATE libSceLibreSsl libc)
    configure_windows_unwind(guest_libressl_runtime_tests)
    add_test(NAME guest_libressl_runtime COMMAND guest_libressl_runtime_tests)
    set_tests_properties(guest_libressl_runtime PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(guest_libressl_runtime PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceLibreSsl>")
    endif()

    add_test_executable(guest_libressl_aes_tests ${LIBS_INCLUDE_DIR}/tests/GuestLibreSslAes.cpp)
    target_include_directories(guest_libressl_aes_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_libressl_aes_tests PRIVATE libSceLibreSsl libc)
    configure_windows_unwind(guest_libressl_aes_tests)
    add_test(NAME guest_libressl_aes COMMAND guest_libressl_aes_tests)
    set_tests_properties(guest_libressl_aes PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(guest_libressl_aes PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceLibreSsl>")
    endif()

    add_test_executable(guest_libressl_error_tests ${LIBS_INCLUDE_DIR}/tests/GuestLibreSslError.cpp)
    target_include_directories(guest_libressl_error_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_libressl_error_tests PRIVATE libSceLibreSsl libc)
    configure_windows_unwind(guest_libressl_error_tests)
    add_test(NAME guest_libressl_error COMMAND guest_libressl_error_tests)
    set_tests_properties(guest_libressl_error PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(guest_libressl_error PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceLibreSsl>")
    endif()

    add_test_executable(guest_libressl_ocsp_tests ${LIBS_INCLUDE_DIR}/tests/GuestLibreSslOcsp.cpp)
    target_include_directories(guest_libressl_ocsp_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_libressl_ocsp_tests PRIVATE libSceLibreSsl libc)
    configure_windows_unwind(guest_libressl_ocsp_tests)
    add_test(NAME guest_libressl_ocsp COMMAND guest_libressl_ocsp_tests)
    set_tests_properties(guest_libressl_ocsp PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(guest_libressl_ocsp PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceLibreSsl>")
    endif()

    add_test_executable(guest_libressl_stack_tests ${LIBS_INCLUDE_DIR}/tests/GuestLibreSslStack.cpp)
    target_include_directories(guest_libressl_stack_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_libressl_stack_tests PRIVATE libSceLibreSsl libc)
    configure_windows_unwind(guest_libressl_stack_tests)
    add_test(NAME guest_libressl_stack COMMAND guest_libressl_stack_tests)
    set_tests_properties(guest_libressl_stack PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(guest_libressl_stack PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceLibreSsl>")
    endif()

    add_test_executable(guest_libressl_ui_tests ${LIBS_INCLUDE_DIR}/tests/GuestLibreSslUi.cpp)
    target_include_directories(guest_libressl_ui_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_libressl_ui_tests PRIVATE libSceLibreSsl libc)
    configure_windows_unwind(guest_libressl_ui_tests)
    add_test(NAME guest_libressl_ui COMMAND guest_libressl_ui_tests)
    set_tests_properties(guest_libressl_ui PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(guest_libressl_ui PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceLibreSsl>")
    endif()

    add_test_executable(guest_libressl_rsa_tests ${LIBS_INCLUDE_DIR}/tests/GuestLibreSslRsa.cpp)
    target_include_directories(guest_libressl_rsa_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_libressl_rsa_tests PRIVATE libSceLibreSsl)
    configure_windows_unwind(guest_libressl_rsa_tests)
    add_test(NAME guest_libressl_rsa COMMAND guest_libressl_rsa_tests)
    set_tests_properties(guest_libressl_rsa PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(guest_libressl_rsa PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceLibreSsl>")
    endif()

    add_test_executable(guest_libressl_x509_store_context_tests ${LIBS_INCLUDE_DIR}/tests/GuestLibreSslX509StoreContext.cpp)
    target_include_directories(guest_libressl_x509_store_context_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_libressl_x509_store_context_tests PRIVATE libSceLibreSsl libc)
    configure_windows_unwind(guest_libressl_x509_store_context_tests)
    add_test(NAME guest_libressl_x509_store_context COMMAND guest_libressl_x509_store_context_tests)
    set_tests_properties(guest_libressl_x509_store_context PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(guest_libressl_x509_store_context PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceLibreSsl>")
    endif()
    add_test_executable(guest_libressl_pkey_type_tests ${LIBS_INCLUDE_DIR}/tests/GuestLibreSslPkeyType.cpp)
    target_include_directories(guest_libressl_pkey_type_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_libressl_pkey_type_tests PRIVATE libSceLibreSsl libc)
    configure_windows_unwind(guest_libressl_pkey_type_tests)
    add_test(NAME guest_libressl_pkey_type COMMAND guest_libressl_pkey_type_tests)
    set_tests_properties(guest_libressl_pkey_type PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(guest_libressl_pkey_type PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceLibreSsl>")
    endif()
endif()

target_sources(libSceLibreSsl PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/Cipher.cpp)

if(BUILD_TESTING)
    add_test_executable(guest_libressl_cipher_tests ${LIBS_INCLUDE_DIR}/tests/GuestLibreSslCipher.cpp)
    target_include_directories(guest_libressl_cipher_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_libressl_cipher_tests PRIVATE libSceLibreSsl libc)
    configure_windows_unwind(guest_libressl_cipher_tests)
    add_test(NAME guest_libressl_cipher COMMAND guest_libressl_cipher_tests)
    set_tests_properties(guest_libressl_cipher PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(guest_libressl_cipher PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceLibreSsl>")
    endif()
endif()

target_sources(libSceLibreSsl PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Alert.cpp)

if(BUILD_TESTING)
    add_test_executable(guest_libressl_alert_tests ${LIBS_INCLUDE_DIR}/tests/GuestLibreSslAlert.cpp)
    target_include_directories(guest_libressl_alert_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_libressl_alert_tests PRIVATE libSceLibreSsl libc)
    configure_windows_unwind(guest_libressl_alert_tests)
    add_test(NAME guest_libressl_alert COMMAND guest_libressl_alert_tests)
    set_tests_properties(guest_libressl_alert PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(guest_libressl_alert PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceLibreSsl>")
    endif()
endif()

if(BUILD_TESTING)
    add_test_executable(guest_libressl_version_tests ${LIBS_INCLUDE_DIR}/tests/GuestLibreSslVersion.cpp)
    target_include_directories(guest_libressl_version_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_libressl_version_tests PRIVATE libSceLibreSsl libc)
    configure_windows_unwind(guest_libressl_version_tests)
    add_test(NAME guest_libressl_version COMMAND guest_libressl_version_tests)
    set_tests_properties(guest_libressl_version PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(guest_libressl_version PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceLibreSsl>")
    endif()
endif()

if(BUILD_TESTING)
    add_test_executable(guest_libressl_crypto_locks_tests ${LIBS_INCLUDE_DIR}/tests/GuestLibreSslCryptoLocks.cpp)
    target_include_directories(guest_libressl_crypto_locks_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_libressl_crypto_locks_tests PRIVATE libSceLibreSsl libc)
    configure_windows_unwind(guest_libressl_crypto_locks_tests)
    add_test(NAME guest_libressl_crypto_locks COMMAND guest_libressl_crypto_locks_tests)
    set_tests_properties(guest_libressl_crypto_locks PROPERTIES TIMEOUT 20)
    if(WIN32)
        set_tests_properties(guest_libressl_crypto_locks PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceLibreSsl>")
    endif()
endif()

target_sources(libSceLibreSsl PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/ObjectTable.cpp ${CMAKE_CURRENT_SOURCE_DIR}/src/ObjectText.cpp)

if(BUILD_TESTING)
    add_test_executable(guest_libressl_object_text_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/ObjectText.cpp)
    target_include_directories(guest_libressl_object_text_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_libressl_object_text_tests PRIVATE libSceLibreSsl libc)
    configure_windows_unwind(guest_libressl_object_text_tests)
    add_test(NAME guest_libressl_object_text COMMAND guest_libressl_object_text_tests)
    set_tests_properties(guest_libressl_object_text PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(guest_libressl_object_text PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceLibreSsl>")
    endif()
endif()

if(BUILD_TESTING)
    add_test_executable(guest_libressl_object_allocation_tests
        tests/ObjectTextAllocation.cpp src/ObjectText.cpp src/ObjectTable.cpp src/Error.cpp)
    target_include_directories(guest_libressl_object_allocation_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_compile_features(guest_libressl_object_allocation_tests PRIVATE cxx_std_23)
    target_link_libraries(guest_libressl_object_allocation_tests PRIVATE libc)
    configure_windows_unwind(guest_libressl_object_allocation_tests)
    add_test(NAME guest_libressl_object_allocation COMMAND guest_libressl_object_allocation_tests)
    set_tests_properties(guest_libressl_object_allocation PROPERTIES TIMEOUT 8)
    if(WIN32)
        set_tests_properties(guest_libressl_object_allocation PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libc>")
    endif()
endif()

target_sources(libSceLibreSsl PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/X509Store.cpp)

if(BUILD_TESTING)
    add_test_executable(guest_libressl_x509_store_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/X509Store.cpp)
    target_include_directories(guest_libressl_x509_store_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_libressl_x509_store_tests PRIVATE libSceLibreSsl libc)
    configure_windows_unwind(guest_libressl_x509_store_tests)
    add_test(NAME guest_libressl_x509_store COMMAND guest_libressl_x509_store_tests)
    add_test(NAME guest_libressl_x509_store_allocator COMMAND guest_libressl_x509_store_tests allocator)
    set_tests_properties(guest_libressl_x509_store guest_libressl_x509_store_allocator PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(guest_libressl_x509_store guest_libressl_x509_store_allocator PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceLibreSsl>")
    endif()
endif()

target_sources(libSceLibreSsl PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/X509VerifyParam.cpp)

if(BUILD_TESTING)
    add_test_executable(guest_libressl_x509_verify_param_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/X509VerifyParam.cpp)
    target_include_directories(guest_libressl_x509_verify_param_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_libressl_x509_verify_param_tests PRIVATE libSceLibreSsl libc)
    configure_windows_unwind(guest_libressl_x509_verify_param_tests)
    add_test(NAME guest_libressl_x509_verify_param COMMAND guest_libressl_x509_verify_param_tests)
    add_test(NAME guest_libressl_x509_verify_param_allocator COMMAND guest_libressl_x509_verify_param_tests allocator)
    set_tests_properties(guest_libressl_x509_verify_param guest_libressl_x509_verify_param_allocator PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(guest_libressl_x509_verify_param guest_libressl_x509_verify_param_allocator PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceLibreSsl>")
    endif()
endif()

if(BUILD_TESTING AND ANYPS5_LIBRESSL_ORACLE)
    include(ExternalProject)
    set(libressl_reference_build "${CMAKE_BINARY_DIR}/libressl-reference-build")
    set(libressl_reference_timestamp)
    if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.24)
        set(libressl_reference_timestamp DOWNLOAD_EXTRACT_TIMESTAMP ON)
    endif()
    ExternalProject_Add(libressl_reference
        URL https://ftp.openbsd.org/pub/OpenBSD/LibreSSL/libressl-2.5.4.tar.gz
        URL_HASH SHA256=107a5b522fbb8318d4c3be668075e5e607296f0a9255d71674caa94571336efa
        ${libressl_reference_timestamp}
        PREFIX "${CMAKE_BINARY_DIR}/libressl-reference"
        BINARY_DIR "${libressl_reference_build}"
        CMAKE_ARGS
            -DCMAKE_BUILD_TYPE=Release
            -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
            -DCMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}
            -DCMAKE_POLICY_VERSION_MINIMUM=3.5
            "-DCMAKE_C_FLAGS=-D_GNU_SOURCE -Wno-error=implicit-function-declaration -Wno-error=incompatible-pointer-types -Wno-error=int-conversion -Wno-error=implicit-int"
        BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> --target crypto ssl
        INSTALL_COMMAND ""
        BUILD_BYPRODUCTS "${libressl_reference_build}/crypto/libcrypto.a" "${libressl_reference_build}/ssl/libssl.a")
    ExternalProject_Get_Property(libressl_reference SOURCE_DIR)
    add_test_executable(guest_libressl_oracle_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/Oracle.cpp ${CMAKE_CURRENT_SOURCE_DIR}/tests/OracleReference.cpp)
    add_dependencies(guest_libressl_oracle_tests libressl_reference)
    set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/tests/OracleReference.cpp PROPERTIES
        INCLUDE_DIRECTORIES "${SOURCE_DIR}/include"
        COMPILE_DEFINITIONS "WIN32_LEAN_AND_MEAN;NOCRYPT")
    target_include_directories(guest_libressl_oracle_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_libressl_oracle_tests PRIVATE libSceLibreSsl libSceLibreSsl3 libc
        "${libressl_reference_build}/ssl/libssl.a" "${libressl_reference_build}/crypto/libcrypto.a")
    if(WIN32)
        target_link_libraries(guest_libressl_oracle_tests PRIVATE ws2_32)
    endif()
    configure_windows_unwind(guest_libressl_oracle_tests)
    add_test(NAME guest_libressl_oracle COMMAND guest_libressl_oracle_tests)
    set_tests_properties(guest_libressl_oracle PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(guest_libressl_oracle PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceLibreSsl>")
    endif()
endif()

if(BUILD_TESTING)
    add_test_executable(guest_libressl_x509_lookup_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/X509Lookup.cpp)
    target_include_directories(guest_libressl_x509_lookup_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_libressl_x509_lookup_tests PRIVATE libSceLibreSsl libc)
    configure_windows_unwind(guest_libressl_x509_lookup_tests)
    add_test(NAME guest_libressl_x509_lookup COMMAND guest_libressl_x509_lookup_tests)
    add_test(NAME guest_libressl_x509_lookup_allocator COMMAND guest_libressl_x509_lookup_tests allocator)
    set_tests_properties(guest_libressl_x509_lookup guest_libressl_x509_lookup_allocator PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(guest_libressl_x509_lookup guest_libressl_x509_lookup_allocator PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceLibreSsl>")
    endif()
endif()
