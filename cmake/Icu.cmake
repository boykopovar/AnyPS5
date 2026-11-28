set(ICU_SOURCE_DIR ${CMAKE_SOURCE_DIR}/3rdparty/icu/icu4c/source)

set(ICU_NORMALIZATION_DIR ${CMAKE_CURRENT_BINARY_DIR}/icu-normalization)
set(ICU_NORMALIZATION_SOURCE ${ICU_NORMALIZATION_DIR}/CommonData.cpp)
file(MAKE_DIRECTORY ${ICU_NORMALIZATION_DIR})
set(ICU_NORMALIZATION_DECLARATIONS "")
set(ICU_NORMALIZATION_ENTRIES "")
set(ICU_NORMALIZATION_FILES nfkc.nrm nfkc_cf.nrm uemoji.icu ulayout.icu)
list(LENGTH ICU_NORMALIZATION_FILES ICU_NORMALIZATION_COUNT)
foreach(ICU_NORMALIZATION_FILE IN LISTS ICU_NORMALIZATION_FILES)
    string(MAKE_C_IDENTIFIER ${ICU_NORMALIZATION_FILE} ICU_NORMALIZATION_NAME)
    set(ICU_NORMALIZATION_INPUT ${ICU_SOURCE_DIR}/data/in/${ICU_NORMALIZATION_FILE})
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${ICU_NORMALIZATION_INPUT})
    file(READ ${ICU_NORMALIZATION_INPUT} ICU_NORMALIZATION_HEX HEX)
    string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," ICU_NORMALIZATION_BYTES "${ICU_NORMALIZATION_HEX}")
    string(APPEND ICU_NORMALIZATION_DECLARATIONS
        "alignas(16) constexpr unsigned char ${ICU_NORMALIZATION_NAME}[]{${ICU_NORMALIZATION_BYTES}};\n")
    string(APPEND ICU_NORMALIZATION_ENTRIES
        "    {U_ICUDATA_NAME \"/${ICU_NORMALIZATION_FILE}\", ${ICU_NORMALIZATION_NAME}},\n")
endforeach()
configure_file(${CMAKE_CURRENT_LIST_DIR}/IcuNormalizationData.cpp.in ${ICU_NORMALIZATION_SOURCE} @ONLY)

add_library(icu_common STATIC EXCLUDE_FROM_ALL
    ${ICU_SOURCE_DIR}/common/appendable.cpp
    ${ICU_SOURCE_DIR}/common/bmpset.cpp
    ${ICU_SOURCE_DIR}/common/brkeng.cpp
    ${ICU_SOURCE_DIR}/common/brkiter.cpp
    ${ICU_SOURCE_DIR}/common/bytesinkutil.cpp
    ${ICU_SOURCE_DIR}/common/bytestream.cpp
    ${ICU_SOURCE_DIR}/common/bytestrie.cpp
    ${ICU_SOURCE_DIR}/common/bytestriebuilder.cpp
    ${ICU_SOURCE_DIR}/common/caniter.cpp
    ${ICU_SOURCE_DIR}/common/characterproperties.cpp
    ${ICU_SOURCE_DIR}/common/chariter.cpp
    ${ICU_SOURCE_DIR}/common/charstr.cpp
    ${ICU_SOURCE_DIR}/common/cmemory.cpp
    ${ICU_SOURCE_DIR}/common/cstring.cpp
    ${ICU_SOURCE_DIR}/common/dictbe.cpp
    ${ICU_SOURCE_DIR}/common/dictionarydata.cpp
    ${ICU_SOURCE_DIR}/common/dtintrv.cpp
    ${ICU_SOURCE_DIR}/common/edits.cpp
    ${ICU_SOURCE_DIR}/common/emojiprops.cpp
    ${ICU_SOURCE_DIR}/common/filteredbrk.cpp
    ${ICU_SOURCE_DIR}/common/filterednormalizer2.cpp
    ${ICU_SOURCE_DIR}/common/loadednormalizer2impl.cpp
    ${ICU_SOURCE_DIR}/common/errorcode.cpp
        ${ICU_SOURCE_DIR}/common/locavailable.cpp
    ${ICU_SOURCE_DIR}/common/locresdata.cpp
        ${ICU_SOURCE_DIR}/common/locmap.cpp
    ${ICU_SOURCE_DIR}/common/normalizer2.cpp
    ${ICU_SOURCE_DIR}/common/normalizer2impl.cpp
    ${ICU_SOURCE_DIR}/common/parsepos.cpp
    ${ICU_SOURCE_DIR}/common/patternprops.cpp
    ${ICU_SOURCE_DIR}/common/propname.cpp
    ${ICU_SOURCE_DIR}/common/putil.cpp
    ${ICU_SOURCE_DIR}/common/rbbidata.cpp
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
    ${ICU_SOURCE_DIR}/common/utext.cpp
    ${ICU_SOURCE_DIR}/common/utf_impl.cpp
    ${ICU_SOURCE_DIR}/common/util.cpp
    ${ICU_SOURCE_DIR}/common/utrace.cpp
    ${ICU_SOURCE_DIR}/common/utrie2.cpp
    ${ICU_SOURCE_DIR}/common/utrie_swap.cpp
    ${ICU_SOURCE_DIR}/common/uvector.cpp
    ${ICU_SOURCE_DIR}/common/uvectr32.cpp
    ${ICU_NORMALIZATION_SOURCE})
target_sources(icu_common PRIVATE
    ${ICU_SOURCE_DIR}/common/fixedstring.cpp
    ${ICU_SOURCE_DIR}/common/localebuilder.cpp
    ${ICU_SOURCE_DIR}/common/locid.cpp
    ${ICU_SOURCE_DIR}/common/locdispnames.cpp
    ${ICU_SOURCE_DIR}/common/locbased.cpp
    ${ICU_SOURCE_DIR}/common/locdspnm.cpp
    ${ICU_SOURCE_DIR}/common/loclikely.cpp
    ${ICU_SOURCE_DIR}/common/loclikelysubtags.cpp
    ${ICU_SOURCE_DIR}/common/locresdata.cpp
    ${ICU_SOURCE_DIR}/common/locutil.cpp
    ${ICU_SOURCE_DIR}/common/lsr.cpp
    ${ICU_SOURCE_DIR}/common/lstmbe.cpp
    ${ICU_SOURCE_DIR}/common/messagepattern.cpp
    ${ICU_SOURCE_DIR}/common/rbbi.cpp
    ${ICU_SOURCE_DIR}/common/rbbi_cache.cpp
    ${ICU_SOURCE_DIR}/common/rbbidata.cpp
    ${ICU_SOURCE_DIR}/common/rbbinode.cpp
    ${ICU_SOURCE_DIR}/common/rbbirb.cpp
    ${ICU_SOURCE_DIR}/common/rbbiscan.cpp
    ${ICU_SOURCE_DIR}/common/rbbisetb.cpp
    ${ICU_SOURCE_DIR}/common/rbbistbl.cpp
    ${ICU_SOURCE_DIR}/common/rbbitblb.cpp
    ${ICU_SOURCE_DIR}/common/resbund.cpp
    ${ICU_SOURCE_DIR}/common/resource.cpp
    ${ICU_SOURCE_DIR}/common/serv.cpp
    ${ICU_SOURCE_DIR}/common/servlk.cpp
    ${ICU_SOURCE_DIR}/common/servlkf.cpp
    ${ICU_SOURCE_DIR}/common/servls.cpp
    ${ICU_SOURCE_DIR}/common/servnotf.cpp
    ${ICU_SOURCE_DIR}/common/servrbf.cpp
    ${ICU_SOURCE_DIR}/common/servslkf.cpp
    ${ICU_SOURCE_DIR}/common/sharedobject.cpp
    ${ICU_SOURCE_DIR}/common/simpleformatter.cpp
    ${ICU_SOURCE_DIR}/common/static_unicode_sets.cpp
    ${ICU_SOURCE_DIR}/common/stringtriebuilder.cpp
    ${ICU_SOURCE_DIR}/common/ubrk.cpp
    ${ICU_SOURCE_DIR}/common/ucasemap.cpp
    ${ICU_SOURCE_DIR}/common/ucasemap_titlecase_brkiter.cpp
    ${ICU_SOURCE_DIR}/common/ucharstriebuilder.cpp
    ${ICU_SOURCE_DIR}/common/uchriter.cpp
    ${ICU_SOURCE_DIR}/common/ucol_swp.cpp
    ${ICU_SOURCE_DIR}/common/ucurr.cpp
    ${ICU_SOURCE_DIR}/common/uhash_us.cpp
    ${ICU_SOURCE_DIR}/common/uiter.cpp
    ${ICU_SOURCE_DIR}/common/ulist.cpp
    ${ICU_SOURCE_DIR}/common/uloc.cpp
    ${ICU_SOURCE_DIR}/common/uloc_keytype.cpp
    ${ICU_SOURCE_DIR}/common/uloc_tag.cpp
    ${ICU_SOURCE_DIR}/common/unifiedcache.cpp
    ${ICU_SOURCE_DIR}/common/uniset_closure.cpp
    ${ICU_SOURCE_DIR}/common/uniset_props.cpp
    ${ICU_SOURCE_DIR}/common/unistr_case_locale.cpp
    ${ICU_SOURCE_DIR}/common/unistr_props.cpp
    ${ICU_SOURCE_DIR}/common/unistr_titlecase_brkiter.cpp
    ${ICU_SOURCE_DIR}/common/uresbund.cpp
    ${ICU_SOURCE_DIR}/common/uresdata.cpp
    ${ICU_SOURCE_DIR}/common/usetiter.cpp
    ${ICU_SOURCE_DIR}/common/uset_props.cpp
    ${ICU_SOURCE_DIR}/common/ustack.cpp
    ${ICU_SOURCE_DIR}/common/ustr_titlecase_brkiter.cpp
    ${ICU_SOURCE_DIR}/common/ustrcase_locale.cpp
    ${ICU_SOURCE_DIR}/common/ustrenum.cpp
    ${ICU_SOURCE_DIR}/common/util_props.cpp
    ${ICU_SOURCE_DIR}/common/utrie.cpp
    ${ICU_SOURCE_DIR}/common/utrie2.cpp
    ${ICU_SOURCE_DIR}/common/utrie2_builder.cpp
    ${ICU_SOURCE_DIR}/common/utypes.cpp
    ${ICU_SOURCE_DIR}/common/uvectr64.cpp)
if(WIN32)
    target_sources(icu_common PRIVATE ${ICU_SOURCE_DIR}/common/wintz.cpp)
endif()
target_include_directories(icu_common SYSTEM PUBLIC ${ICU_SOURCE_DIR}/common)
target_compile_definitions(icu_common PUBLIC U_STATIC_IMPLEMENTATION PRIVATE U_COMMON_IMPLEMENTATION UCONFIG_NO_FILE_IO=1 UCONFIG_USE_WINDOWS_LCID_MAPPING_API=0)
target_compile_features(icu_common PUBLIC cxx_std_17)
target_sources(icu_common PRIVATE ${ICU_SOURCE_DIR}/common/uset.cpp)
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

target_sources(icu_i18n PRIVATE
    ${ICU_SOURCE_DIR}/i18n/ufieldpositer.cpp
    ${ICU_SOURCE_DIR}/i18n/fpositer.cpp
    ${ICU_SOURCE_DIR}/i18n/fphdlimp.cpp
    ${ICU_SOURCE_DIR}/i18n/astro.cpp
    ${ICU_SOURCE_DIR}/i18n/basictz.cpp
    ${ICU_SOURCE_DIR}/i18n/bocsu.cpp
    ${ICU_SOURCE_DIR}/i18n/buddhcal.cpp
    ${ICU_SOURCE_DIR}/i18n/calendar.cpp
    ${ICU_SOURCE_DIR}/i18n/cecal.cpp
    ${ICU_SOURCE_DIR}/i18n/chnsecal.cpp
    ${ICU_SOURCE_DIR}/i18n/choicfmt.cpp
    ${ICU_SOURCE_DIR}/i18n/coleitr.cpp
    ${ICU_SOURCE_DIR}/i18n/coll.cpp
    ${ICU_SOURCE_DIR}/i18n/collation.cpp
    ${ICU_SOURCE_DIR}/i18n/collationbuilder.cpp
    ${ICU_SOURCE_DIR}/i18n/collationcompare.cpp
    ${ICU_SOURCE_DIR}/i18n/collationdata.cpp
    ${ICU_SOURCE_DIR}/i18n/collationdatabuilder.cpp
    ${ICU_SOURCE_DIR}/i18n/collationdatareader.cpp
    ${ICU_SOURCE_DIR}/i18n/collationdatawriter.cpp
    ${ICU_SOURCE_DIR}/i18n/collationfastlatin.cpp
    ${ICU_SOURCE_DIR}/i18n/collationfastlatinbuilder.cpp
    ${ICU_SOURCE_DIR}/i18n/collationfcd.cpp
    ${ICU_SOURCE_DIR}/i18n/collationiterator.cpp
    ${ICU_SOURCE_DIR}/i18n/collationkeys.cpp
    ${ICU_SOURCE_DIR}/i18n/collationroot.cpp
    ${ICU_SOURCE_DIR}/i18n/collationrootelements.cpp
    ${ICU_SOURCE_DIR}/i18n/collationruleparser.cpp
    ${ICU_SOURCE_DIR}/i18n/collationsets.cpp
    ${ICU_SOURCE_DIR}/i18n/collationsettings.cpp
    ${ICU_SOURCE_DIR}/i18n/collationtailoring.cpp
    ${ICU_SOURCE_DIR}/i18n/collationweights.cpp
    ${ICU_SOURCE_DIR}/i18n/coptccal.cpp
    ${ICU_SOURCE_DIR}/i18n/curramt.cpp
    ${ICU_SOURCE_DIR}/i18n/currpinf.cpp
    ${ICU_SOURCE_DIR}/i18n/currunit.cpp
    ${ICU_SOURCE_DIR}/i18n/dangical.cpp
    ${ICU_SOURCE_DIR}/i18n/datefmt.cpp
    ${ICU_SOURCE_DIR}/i18n/dayperiodrules.cpp
    ${ICU_SOURCE_DIR}/i18n/dcfmtsym.cpp
    ${ICU_SOURCE_DIR}/i18n/decContext.cpp
    ${ICU_SOURCE_DIR}/i18n/decNumber.cpp
    ${ICU_SOURCE_DIR}/i18n/decimfmt.cpp
    ${ICU_SOURCE_DIR}/i18n/displayoptions.cpp
    ${ICU_SOURCE_DIR}/i18n/double-conversion-bignum.cpp
    ${ICU_SOURCE_DIR}/i18n/double-conversion-bignum-dtoa.cpp
    ${ICU_SOURCE_DIR}/i18n/double-conversion-cached-powers.cpp
    ${ICU_SOURCE_DIR}/i18n/double-conversion-double-to-string.cpp
    ${ICU_SOURCE_DIR}/i18n/double-conversion-fast-dtoa.cpp
    ${ICU_SOURCE_DIR}/i18n/double-conversion-string-to-double.cpp
    ${ICU_SOURCE_DIR}/i18n/double-conversion-strtod.cpp
    ${ICU_SOURCE_DIR}/i18n/dtfmtsym.cpp
    ${ICU_SOURCE_DIR}/i18n/dtitvfmt.cpp
    ${ICU_SOURCE_DIR}/i18n/dtitvinf.cpp
    ${ICU_SOURCE_DIR}/i18n/dtptngen.cpp
    ${ICU_SOURCE_DIR}/i18n/dtrule.cpp
    ${ICU_SOURCE_DIR}/i18n/erarules.cpp
    ${ICU_SOURCE_DIR}/i18n/ethpccal.cpp
    ${ICU_SOURCE_DIR}/i18n/fmtable.cpp
    ${ICU_SOURCE_DIR}/i18n/format.cpp
    ${ICU_SOURCE_DIR}/i18n/formatted_string_builder.cpp
    ${ICU_SOURCE_DIR}/i18n/formattedval_iterimpl.cpp
    ${ICU_SOURCE_DIR}/i18n/formattedval_sbimpl.cpp
    ${ICU_SOURCE_DIR}/i18n/formattedvalue.cpp
    ${ICU_SOURCE_DIR}/i18n/gregocal.cpp
    ${ICU_SOURCE_DIR}/i18n/gregoimp.cpp
    ${ICU_SOURCE_DIR}/i18n/hebrwcal.cpp
    ${ICU_SOURCE_DIR}/i18n/indiancal.cpp
    ${ICU_SOURCE_DIR}/i18n/islamcal.cpp
    ${ICU_SOURCE_DIR}/i18n/iso8601cal.cpp
    ${ICU_SOURCE_DIR}/i18n/japancal.cpp
    ${ICU_SOURCE_DIR}/i18n/listformatter.cpp
    ${ICU_SOURCE_DIR}/i18n/measure.cpp
    ${ICU_SOURCE_DIR}/i18n/measunit.cpp
    ${ICU_SOURCE_DIR}/i18n/measunit_extra.cpp
    ${ICU_SOURCE_DIR}/i18n/number_affixutils.cpp
    ${ICU_SOURCE_DIR}/i18n/number_asformat.cpp
    ${ICU_SOURCE_DIR}/i18n/number_compact.cpp
    ${ICU_SOURCE_DIR}/i18n/number_currencysymbols.cpp
    ${ICU_SOURCE_DIR}/i18n/number_decimalquantity.cpp
    ${ICU_SOURCE_DIR}/i18n/number_decimfmtprops.cpp
    ${ICU_SOURCE_DIR}/i18n/number_fluent.cpp
    ${ICU_SOURCE_DIR}/i18n/number_formatimpl.cpp
    ${ICU_SOURCE_DIR}/i18n/number_grouping.cpp
    ${ICU_SOURCE_DIR}/i18n/number_integerwidth.cpp
    ${ICU_SOURCE_DIR}/i18n/number_longnames.cpp
    ${ICU_SOURCE_DIR}/i18n/number_mapper.cpp
    ${ICU_SOURCE_DIR}/i18n/number_modifiers.cpp
    ${ICU_SOURCE_DIR}/i18n/number_multiplier.cpp
    ${ICU_SOURCE_DIR}/i18n/number_notation.cpp
    ${ICU_SOURCE_DIR}/i18n/number_output.cpp
    ${ICU_SOURCE_DIR}/i18n/number_padding.cpp
    ${ICU_SOURCE_DIR}/i18n/number_patternmodifier.cpp
    ${ICU_SOURCE_DIR}/i18n/number_patternstring.cpp
    ${ICU_SOURCE_DIR}/i18n/number_rounding.cpp
    ${ICU_SOURCE_DIR}/i18n/number_scientific.cpp
    ${ICU_SOURCE_DIR}/i18n/number_simple.cpp
    ${ICU_SOURCE_DIR}/i18n/number_skeletons.cpp
    ${ICU_SOURCE_DIR}/i18n/number_symbolswrapper.cpp
    ${ICU_SOURCE_DIR}/i18n/number_usageprefs.cpp
    ${ICU_SOURCE_DIR}/i18n/number_utils.cpp
    ${ICU_SOURCE_DIR}/i18n/numfmt.cpp
    ${ICU_SOURCE_DIR}/i18n/numparse_affixes.cpp
    ${ICU_SOURCE_DIR}/i18n/numparse_compositions.cpp
    ${ICU_SOURCE_DIR}/i18n/numparse_currency.cpp
    ${ICU_SOURCE_DIR}/i18n/numparse_decimal.cpp
    ${ICU_SOURCE_DIR}/i18n/numparse_impl.cpp
    ${ICU_SOURCE_DIR}/i18n/numparse_parsednumber.cpp
    ${ICU_SOURCE_DIR}/i18n/numparse_scientific.cpp
    ${ICU_SOURCE_DIR}/i18n/numparse_symbols.cpp
    ${ICU_SOURCE_DIR}/i18n/numparse_validators.cpp
    ${ICU_SOURCE_DIR}/i18n/numsys.cpp
    ${ICU_SOURCE_DIR}/i18n/olsontz.cpp
    ${ICU_SOURCE_DIR}/i18n/persncal.cpp
    ${ICU_SOURCE_DIR}/i18n/pluralranges.cpp
    ${ICU_SOURCE_DIR}/i18n/plurfmt.cpp
    ${ICU_SOURCE_DIR}/i18n/plurrule.cpp
    ${ICU_SOURCE_DIR}/i18n/msgfmt.cpp
    ${ICU_SOURCE_DIR}/i18n/nfrs.cpp
    ${ICU_SOURCE_DIR}/i18n/nfrule.cpp
    ${ICU_SOURCE_DIR}/i18n/nfsubs.cpp
    ${ICU_SOURCE_DIR}/i18n/rbnf.cpp
    ${ICU_SOURCE_DIR}/i18n/rbtz.cpp
    ${ICU_SOURCE_DIR}/i18n/region.cpp
    ${ICU_SOURCE_DIR}/i18n/reldtfmt.cpp
    ${ICU_SOURCE_DIR}/i18n/rulebasedcollator.cpp
    ${ICU_SOURCE_DIR}/i18n/selfmt.cpp
    ${ICU_SOURCE_DIR}/i18n/simpletz.cpp
    ${ICU_SOURCE_DIR}/i18n/smpdtfmt.cpp
    ${ICU_SOURCE_DIR}/i18n/smpdtfst.cpp
    ${ICU_SOURCE_DIR}/i18n/sortkey.cpp
    ${ICU_SOURCE_DIR}/i18n/ucol.cpp
    ${ICU_SOURCE_DIR}/i18n/ucol_res.cpp
    ${ICU_SOURCE_DIR}/i18n/ucol_sit.cpp
    ${ICU_SOURCE_DIR}/i18n/ucoleitr.cpp
    ${ICU_SOURCE_DIR}/i18n/uitercollationiterator.cpp
    ${ICU_SOURCE_DIR}/i18n/umsg.cpp
    ${ICU_SOURCE_DIR}/i18n/utf16collationiterator.cpp
    ${ICU_SOURCE_DIR}/i18n/utf8collationiterator.cpp
    ${ICU_SOURCE_DIR}/i18n/standardplural.cpp
    ${ICU_SOURCE_DIR}/i18n/string_segment.cpp
    ${ICU_SOURCE_DIR}/i18n/taiwncal.cpp
    ${ICU_SOURCE_DIR}/i18n/timezone.cpp
    ${ICU_SOURCE_DIR}/i18n/tzfmt.cpp
    ${ICU_SOURCE_DIR}/i18n/tzgnames.cpp
    ${ICU_SOURCE_DIR}/i18n/tznames.cpp
    ${ICU_SOURCE_DIR}/i18n/tznames_impl.cpp
    ${ICU_SOURCE_DIR}/i18n/tzrule.cpp
    ${ICU_SOURCE_DIR}/i18n/tztrans.cpp
    ${ICU_SOURCE_DIR}/i18n/ucal.cpp
    ${ICU_SOURCE_DIR}/i18n/udateintervalformat.cpp
    ${ICU_SOURCE_DIR}/i18n/units_complexconverter.cpp
    ${ICU_SOURCE_DIR}/i18n/units_converter.cpp
    ${ICU_SOURCE_DIR}/i18n/units_data.cpp
    ${ICU_SOURCE_DIR}/i18n/units_router.cpp
    ${ICU_SOURCE_DIR}/i18n/vtzone.cpp
    ${ICU_SOURCE_DIR}/i18n/zonemeta.cpp)
