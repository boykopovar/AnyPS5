set(ICU_SOURCE_DIR ${CMAKE_SOURCE_DIR}/3rdparty/icu/icu4c/source)

add_library(icu_common STATIC EXCLUDE_FROM_ALL
    ${ICU_SOURCE_DIR}/common/appendable.cpp
    ${ICU_SOURCE_DIR}/common/bmpset.cpp
    ${ICU_SOURCE_DIR}/common/bytesinkutil.cpp
    ${ICU_SOURCE_DIR}/common/bytestream.cpp
    ${ICU_SOURCE_DIR}/common/bytestrie.cpp
    ${ICU_SOURCE_DIR}/common/characterproperties.cpp
    ${ICU_SOURCE_DIR}/common/charstr.cpp
    ${ICU_SOURCE_DIR}/common/cmemory.cpp
    ${ICU_SOURCE_DIR}/common/cstring.cpp
    ${ICU_SOURCE_DIR}/common/edits.cpp
    ${ICU_SOURCE_DIR}/common/emojiprops.cpp
    ${ICU_SOURCE_DIR}/common/filterednormalizer2.cpp
    ${ICU_SOURCE_DIR}/common/loadednormalizer2impl.cpp
    ${ICU_SOURCE_DIR}/common/locmap.cpp
    ${ICU_SOURCE_DIR}/common/normalizer2.cpp
    ${ICU_SOURCE_DIR}/common/normalizer2impl.cpp
    ${ICU_SOURCE_DIR}/common/parsepos.cpp
    ${ICU_SOURCE_DIR}/common/patternprops.cpp
    ${ICU_SOURCE_DIR}/common/propname.cpp
    ${ICU_SOURCE_DIR}/common/putil.cpp
    ${ICU_SOURCE_DIR}/common/ruleiter.cpp
    ${ICU_SOURCE_DIR}/common/stringpiece.cpp
    ${ICU_SOURCE_DIR}/common/uarrsort.cpp
    ${ICU_SOURCE_DIR}/common/ubidi.cpp
    ${ICU_SOURCE_DIR}/common/ubidi_props.cpp
    ${ICU_SOURCE_DIR}/common/ubidiln.cpp
    ${ICU_SOURCE_DIR}/common/ubidiwrt.cpp
    ${ICU_SOURCE_DIR}/common/ucnv.cpp
    ${ICU_SOURCE_DIR}/common/ucnv2022.cpp
    ${ICU_SOURCE_DIR}/common/ucnv_bld.cpp
    ${ICU_SOURCE_DIR}/common/ucnv_cb.cpp
    ${ICU_SOURCE_DIR}/common/ucnv_cnv.cpp
    ${ICU_SOURCE_DIR}/common/ucnv_ct.cpp
    ${ICU_SOURCE_DIR}/common/ucnv_err.cpp
    ${ICU_SOURCE_DIR}/common/ucnv_ext.cpp
    ${ICU_SOURCE_DIR}/common/ucnv_io.cpp
    ${ICU_SOURCE_DIR}/common/ucnv_lmb.cpp
    ${ICU_SOURCE_DIR}/common/ucnv_set.cpp
    ${ICU_SOURCE_DIR}/common/ucnv_u16.cpp
    ${ICU_SOURCE_DIR}/common/ucnv_u32.cpp
    ${ICU_SOURCE_DIR}/common/ucnv_u7.cpp
    ${ICU_SOURCE_DIR}/common/ucnv_u8.cpp
    ${ICU_SOURCE_DIR}/common/ucnvbocu.cpp
    ${ICU_SOURCE_DIR}/common/ucnvhz.cpp
    ${ICU_SOURCE_DIR}/common/ucnvisci.cpp
    ${ICU_SOURCE_DIR}/common/ucnvlat1.cpp
    ${ICU_SOURCE_DIR}/common/ucnvmbcs.cpp
    ${ICU_SOURCE_DIR}/common/ucnvscsu.cpp
    ${ICU_SOURCE_DIR}/common/uenum.cpp
    ${ICU_SOURCE_DIR}/common/ustr_cnv.cpp
    ${ICU_SOURCE_DIR}/common/ustrfmt.cpp
    ${ICU_SOURCE_DIR}/common/ucase.cpp
    ${ICU_SOURCE_DIR}/common/uchar.cpp
    ${ICU_SOURCE_DIR}/common/ucharstrie.cpp
    ${ICU_SOURCE_DIR}/common/ucharstrieiterator.cpp
    ${ICU_SOURCE_DIR}/common/ucln_cmn.cpp
    ${ICU_SOURCE_DIR}/common/ucmndata.cpp
    ${ICU_SOURCE_DIR}/common/ucptrie.cpp
    ${ICU_SOURCE_DIR}/common/udata.cpp
    ${ICU_SOURCE_DIR}/common/udatamem.cpp
    ${ICU_SOURCE_DIR}/common/udataswp.cpp
    ${ICU_SOURCE_DIR}/common/uhash.cpp
    ${ICU_SOURCE_DIR}/common/uinvchar.cpp
    ${ICU_SOURCE_DIR}/common/umapfile.cpp
    ${ICU_SOURCE_DIR}/common/umath.cpp
    ${ICU_SOURCE_DIR}/common/umutablecptrie.cpp
    ${ICU_SOURCE_DIR}/common/umutex.cpp
    ${ICU_SOURCE_DIR}/common/unames.cpp
    ${ICU_SOURCE_DIR}/common/unifilt.cpp
    ${ICU_SOURCE_DIR}/common/unifunct.cpp
    ${ICU_SOURCE_DIR}/common/uniset.cpp
    ${ICU_SOURCE_DIR}/common/uniset_props.cpp
    ${ICU_SOURCE_DIR}/common/unisetspan.cpp
    ${ICU_SOURCE_DIR}/common/unistr.cpp
    ${ICU_SOURCE_DIR}/common/unistr_case.cpp
    ${ICU_SOURCE_DIR}/common/unormcmp.cpp
    ${ICU_SOURCE_DIR}/common/uobject.cpp
    ${ICU_SOURCE_DIR}/common/uprops.cpp
    ${ICU_SOURCE_DIR}/common/uscript_props.cpp
    ${ICU_SOURCE_DIR}/common/ustrcase.cpp
    ${ICU_SOURCE_DIR}/common/ustring.cpp
    ${ICU_SOURCE_DIR}/common/ustrtrns.cpp
    ${ICU_SOURCE_DIR}/common/utf_impl.cpp
    ${ICU_SOURCE_DIR}/common/util.cpp
    ${ICU_SOURCE_DIR}/common/utrace.cpp
    ${ICU_SOURCE_DIR}/common/utrie2.cpp
    ${ICU_SOURCE_DIR}/common/utrie_swap.cpp
    ${ICU_SOURCE_DIR}/common/uvector.cpp
    ${ICU_SOURCE_DIR}/common/uvectr32.cpp
    ${ICU_SOURCE_DIR}/stubdata/stubdata.cpp)
if(WIN32)
    target_sources(icu_common PRIVATE
        ${ICU_SOURCE_DIR}/common/fixedstring.cpp
        ${ICU_SOURCE_DIR}/common/localebuilder.cpp
        ${ICU_SOURCE_DIR}/common/locid.cpp
        ${ICU_SOURCE_DIR}/common/loclikely.cpp
        ${ICU_SOURCE_DIR}/common/loclikelysubtags.cpp
        ${ICU_SOURCE_DIR}/common/lsr.cpp
        ${ICU_SOURCE_DIR}/common/resource.cpp
        ${ICU_SOURCE_DIR}/common/ucol_swp.cpp
        ${ICU_SOURCE_DIR}/common/uloc.cpp
        ${ICU_SOURCE_DIR}/common/uloc_keytype.cpp
        ${ICU_SOURCE_DIR}/common/uloc_tag.cpp
        ${ICU_SOURCE_DIR}/common/uresbund.cpp
        ${ICU_SOURCE_DIR}/common/uresdata.cpp
        ${ICU_SOURCE_DIR}/common/ustrenum.cpp
        ${ICU_SOURCE_DIR}/common/utypes.cpp
        ${ICU_SOURCE_DIR}/common/wintz.cpp)
endif()
target_include_directories(icu_common SYSTEM PUBLIC ${ICU_SOURCE_DIR}/common)
target_compile_definitions(icu_common PUBLIC U_STATIC_IMPLEMENTATION PRIVATE U_COMMON_IMPLEMENTATION UCONFIG_NO_FILE_IO=1 UCONFIG_USE_WINDOWS_LCID_MAPPING_API=0)
target_compile_features(icu_common PUBLIC cxx_std_17)
set_target_properties(icu_common PROPERTIES POSITION_INDEPENDENT_CODE ON CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN ON)
if(MINGW OR CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_options(icu_common INTERFACE LINKER:--exclude-libs,libicu_common.a)
endif()

add_library(icu_i18n STATIC EXCLUDE_FROM_ALL
    ${ICU_SOURCE_DIR}/i18n/csdetect.cpp
    ${ICU_SOURCE_DIR}/i18n/csmatch.cpp
    ${ICU_SOURCE_DIR}/i18n/csr2022.cpp
    ${ICU_SOURCE_DIR}/i18n/csrecog.cpp
    ${ICU_SOURCE_DIR}/i18n/csrmbcs.cpp
    ${ICU_SOURCE_DIR}/i18n/csrsbcs.cpp
    ${ICU_SOURCE_DIR}/i18n/csrucode.cpp
    ${ICU_SOURCE_DIR}/i18n/csrutf8.cpp
    ${ICU_SOURCE_DIR}/i18n/inputext.cpp
    ${ICU_SOURCE_DIR}/i18n/ucln_in.cpp
    ${ICU_SOURCE_DIR}/i18n/ucsdet.cpp)
target_include_directories(icu_i18n SYSTEM PUBLIC ${ICU_SOURCE_DIR}/i18n)
target_compile_definitions(icu_i18n PRIVATE U_I18N_IMPLEMENTATION UCONFIG_NO_FILE_IO=1)
target_link_libraries(icu_i18n PUBLIC icu_common)
set_target_properties(icu_i18n PROPERTIES POSITION_INDEPENDENT_CODE ON CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN ON)
if(MINGW OR CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_options(icu_i18n INTERFACE LINKER:--exclude-libs,libicu_i18n.a)
endif()
