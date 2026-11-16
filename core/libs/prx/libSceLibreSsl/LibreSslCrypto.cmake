set(LIBRESSL_ROOT ${PROJECT_SOURCE_DIR}/3rdparty/libressl-openbsd/src/lib/libcrypto)
if(NOT EXISTS ${LIBRESSL_ROOT}/asn1/a_strex.c OR NOT EXISTS ${LIBRESSL_ROOT}/objects/objects.txt)
    message(FATAL_ERROR "libSceLibreSsl builds X509_NAME_print_ex from 3rdparty/libressl-openbsd; initialize it with git submodule update --init 3rdparty/libressl-openbsd")
endif()
set(LIBRESSL_GEN ${CMAKE_CURRENT_BINARY_DIR}/libressl-gen)
set(LIBRESSL_INC ${LIBRESSL_GEN}/include)
set(LIBRESSL_OPENSSL_INC ${LIBRESSL_INC}/openssl)

set(LIBRESSL_SUBDIRS
    asn1 bio bn buffer cms comp conf dh dsa dso ec ecdh ecdsa engine err
    objects ocsp pem pkcs12 pkcs7 rand rsa stack ts ui x509 lhash util)
set(LIBRESSL_INCLUDE_DIRS ${LIBRESSL_INC} ${LIBRESSL_ROOT} ${LIBRESSL_OPENSSL_INC})
foreach(dir IN LISTS LIBRESSL_SUBDIRS)
    list(APPEND LIBRESSL_INCLUDE_DIRS ${LIBRESSL_ROOT}/${dir})
endforeach()
list(APPEND LIBRESSL_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/libressl)

if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|amd64")
    set(LIBRESSL_ARCH amd64)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64")
    set(LIBRESSL_ARCH aarch64)
else()
    message(FATAL_ERROR "libressl_crypto: unsupported processor ${CMAKE_SYSTEM_PROCESSOR}")
endif()

file(GLOB LIBRESSL_PUBLIC_HEADERS
    ${LIBRESSL_ROOT}/*/*.h
    ${LIBRESSL_ROOT}/*.h)
list(FILTER LIBRESSL_PUBLIC_HEADERS EXCLUDE REGEX ".*/arch/.*/opensslconf\\.h$")
file(MAKE_DIRECTORY ${LIBRESSL_OPENSSL_INC})
foreach(header IN LISTS LIBRESSL_PUBLIC_HEADERS)
    get_filename_component(name ${header} NAME)
    configure_file(${header} ${LIBRESSL_OPENSSL_INC}/${name} COPYONLY)
endforeach()
configure_file(${LIBRESSL_ROOT}/arch/${LIBRESSL_ARCH}/opensslconf.h
    ${LIBRESSL_OPENSSL_INC}/opensslconf.h COPYONLY)

find_package(Python3 REQUIRED COMPONENTS Interpreter)

add_custom_command(
    OUTPUT ${LIBRESSL_OPENSSL_INC}/obj_mac.h ${LIBRESSL_OPENSSL_INC}/obj_dat.h
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/GenerateObjects.py
        ${LIBRESSL_ROOT}/objects/objects.txt ${LIBRESSL_ROOT}/objects/obj_mac.num
        ${LIBRESSL_OPENSSL_INC}/obj_mac.h ${LIBRESSL_OPENSSL_INC}/obj_dat.h
    DEPENDS ${LIBRESSL_ROOT}/objects/objects.txt ${LIBRESSL_ROOT}/objects/obj_mac.num
        ${CMAKE_CURRENT_SOURCE_DIR}/GenerateObjects.py
    COMMENT "Generating LibreSSL object tables"
    VERBATIM)

set(LIBRESSL_CRYPTO_SOURCES
    cryptlib.c crypto_lock.c cpt_err.c ex_data.c malloc-wrapper.c mem_dbg.c o_time.c
    asn1/a_bitstr.c asn1/a_bool.c asn1/a_dup.c asn1/a_enum.c asn1/a_int.c
    asn1/a_mbstr.c asn1/a_object.c asn1/a_octet.c asn1/a_print.c asn1/a_strex.c
    asn1/a_strnid.c asn1/a_time.c asn1/a_type.c asn1/a_utf8.c asn1/asn1_lib.c
    asn1/asn1_err.c asn1/asn_pack.c asn1/tasn_dec.c asn1/tasn_enc.c asn1/tasn_fre.c
    asn1/tasn_new.c asn1/tasn_typ.c asn1/tasn_utl.c asn1/x_name.c
    bio/b_dump.c bio/b_print.c bio/bio_err.c bio/bio_lib.c bio/bio_meth.c
    bio/bss_fd.c bio/bss_file.c bio/bss_mem.c
    bn/bn_add.c bn/bn_asm.c bn/bn_blind.c bn/bn_const.c bn/bn_ctx.c bn/bn_depr.c
    bn/bn_div.c bn/bn_err.c bn/bn_exp.c bn/bn_exp2.c bn/bn_gcd.c bn/bn_gf2m.c
    bn/bn_kron.c bn/bn_lib.c bn/bn_mod.c bn/bn_mont.c bn/bn_mpi.c bn/bn_mul.c
    bn/bn_prime.c bn/bn_print.c bn/bn_recp.c bn/bn_shift.c bn/bn_sqr.c
    bn/bn_sqrt.c bn/bn_word.c bn/bn_x931p.c
    buffer/buf_err.c buffer/buf_str.c buffer/buffer.c
    cms/cms_err.c comp/comp_err.c conf/conf_err.c dh/dh_err.c dsa/dsa_err.c
    dso/dso_err.c ec/ec_err.c ecdh/ech_err.c ecdsa/ecs_err.c engine/eng_err.c
    err/err.c err/err_all.c err/err_prn.c evp/evp_err.c gost/gost_err.c
    objects/o_names.c objects/obj_dat.c objects/obj_err.c objects/obj_lib.c
    objects/obj_xref.c ocsp/ocsp_err.c pem/pem_err.c pkcs12/pk12err.c
    pkcs7/pkcs7err.c rand/rand_err.c rsa/rsa_err.c stack/stack.c ts/ts_err.c
    ui/ui_err.c x509/x509_err.c x509/x509_obj.c x509/x509name.c
    lhash/lhash.c lhash/lh_stats.c)

set(LIBRESSL_SRC_FILES)
foreach(rel IN LISTS LIBRESSL_CRYPTO_SOURCES)
    list(APPEND LIBRESSL_SRC_FILES ${LIBRESSL_ROOT}/${rel})
endforeach()

add_library(libressl_crypto STATIC EXCLUDE_FROM_ALL
    ${CMAKE_CURRENT_SOURCE_DIR}/libressl/libressl_port.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libressl/strlcpy.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libressl/strlcat.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libressl/reallocarray.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libressl/recallocarray.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libressl/freezero.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libressl/glue.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libressl/x509_name_print.c
    ${LIBRESSL_SRC_FILES}
    ${LIBRESSL_OPENSSL_INC}/obj_mac.h
    ${LIBRESSL_OPENSSL_INC}/obj_dat.h)

set_target_properties(libressl_crypto PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    C_VISIBILITY_PRESET hidden)

target_include_directories(libressl_crypto PRIVATE ${LIBRESSL_INCLUDE_DIRS})
target_include_directories(libressl_crypto SYSTEM PRIVATE ${LIBRESSL_INC})
target_compile_definitions(libressl_crypto PRIVATE LIBRESSL_INTERNAL OPENSSL_NO_ASM)
target_compile_options(libressl_crypto PRIVATE
    "-include;libressl_port.h"
    "-ffunction-sections" "-fdata-sections")

find_package(Threads REQUIRED)
target_link_libraries(libressl_crypto PUBLIC Threads::Threads)
target_include_directories(libSceLibreSsl PRIVATE ${LIBRESSL_INC})
target_link_libraries(libSceLibreSsl PRIVATE libressl_crypto)
