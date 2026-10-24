enable_language(C)
set(JXR_SOURCE_DIR ${CMAKE_SOURCE_DIR}/3rdparty/jxrlib)

function(configure_jxr_native target)
    target_include_directories(${target} SYSTEM PRIVATE
        ${JXR_SOURCE_DIR}/common/include ${JXR_SOURCE_DIR}/image/sys ${JXR_SOURCE_DIR}/jxrgluelib)
    target_compile_definitions(${target} PRIVATE DISABLE_PERF_MEASUREMENT)
    if(WIN32)
        target_compile_definitions(${target} PRIVATE WIN32 __in=)
        target_compile_options(${target} PRIVATE -include ${JXR_SOURCE_DIR}/image/sys/strcodec.h)
    else()
        target_compile_definitions(${target} PRIVATE __ANSI__)
    endif()
    set_target_properties(${target} PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON
        POSITION_INDEPENDENT_CODE ON C_VISIBILITY_PRESET hidden)
endfunction()

add_library(jxr STATIC EXCLUDE_FROM_ALL
    ${JXR_SOURCE_DIR}/image/sys/adapthuff.c
    ${JXR_SOURCE_DIR}/image/sys/image.c
    ${JXR_SOURCE_DIR}/image/sys/strcodec.c
    ${JXR_SOURCE_DIR}/image/sys/strPredQuant.c
    ${JXR_SOURCE_DIR}/image/sys/strTransform.c
    ${JXR_SOURCE_DIR}/image/decode/decode.c
    ${JXR_SOURCE_DIR}/image/decode/JXRTranscode.c
    ${JXR_SOURCE_DIR}/image/decode/postprocess.c
    ${JXR_SOURCE_DIR}/image/decode/segdec.c
    ${JXR_SOURCE_DIR}/image/decode/strdec.c
    ${JXR_SOURCE_DIR}/image/decode/strInvTransform.c
    ${JXR_SOURCE_DIR}/image/decode/strPredQuantDec.c
    ${JXR_SOURCE_DIR}/image/encode/encode.c
    ${JXR_SOURCE_DIR}/image/encode/segenc.c
    ${JXR_SOURCE_DIR}/image/encode/strenc.c
    ${JXR_SOURCE_DIR}/image/encode/strFwdTransform.c
    ${JXR_SOURCE_DIR}/image/encode/strPredQuantEnc.c
    ${JXR_SOURCE_DIR}/jxrgluelib/JXRGlue.c
    ${JXR_SOURCE_DIR}/jxrgluelib/JXRGlueJxr.c
    ${JXR_SOURCE_DIR}/jxrgluelib/JXRGluePFC.c
    ${JXR_SOURCE_DIR}/jxrgluelib/JXRMeta.c)
configure_jxr_native(jxr)
if(UNIX)
    target_link_libraries(jxr PRIVATE m)
endif()
