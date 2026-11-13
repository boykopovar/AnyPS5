add_library(libSceIcu SHARED EXCLUDE_FROM_ALL CharacterDigits.cpp Comparisons.cpp ConstrainedFieldPosition.cpp Enumeration.cpp ErrorName.cpp Export.cpp Invariant.cpp Iterator.cpp Numeric.cpp Set.cpp Tokenize.cpp Unescape.cpp Version.cpp Utf32.cpp JavaUtf8.cpp Compare.cpp Search.cpp Character.cpp Configuration.cpp LocaleCore.cpp Copy.cpp)
target_include_directories(libSceIcu PRIVATE ${LIBS_INCLUDE_DIR})
target_sources(libSceIcu PRIVATE RecoveredInventory.cpp)
configure_windows_unwind(libSceIcu)
target_link_libraries(libSceIcu PRIVATE libc)
target_sources(libSceIcu PRIVATE ConverterCallbacks.cpp)
target_sources(libSceIcu PRIVATE LegacyExports.cpp)
target_sources(libSceIcu PRIVATE CString.cpp)
target_sources(libSceIcu PRIVATE CompareIter.cpp)
target_sources(libSceIcu PRIVATE Space.cpp)
target_sources(libSceIcu PRIVATE Utf8.cpp)
target_sources(libSceIcu PRIVATE Resource.cpp)
target_sources(libSceIcu PRIVATE Properties.cpp CaseMapping.cpp)
target_sources(libSceIcu PRIVATE Bidi.cpp CharsetDetection.cpp Normalization.cpp CodePoints.cpp)
target_link_libraries(libSceIcu PRIVATE icu_common icu_i18n)
target_sources(libSceIcu PRIVATE Locale.cpp LocaleMap.cpp)
target_sources(libSceIcu PRIVATE TerminateBuffer.cpp)
target_sources(libSceIcu PRIVATE LanguageTag.cpp)
target_sources(libSceIcu PRIVATE Script.cpp)

if(BUILD_TESTING)
    add_test_executable(guest_icu_language_tag_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/LanguageTag.cpp)
    target_include_directories(guest_icu_language_tag_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_icu_language_tag_tests PRIVATE libSceIcu)
    configure_windows_unwind(guest_icu_language_tag_tests)
    add_test(NAME guest_icu_language_tag COMMAND guest_icu_language_tag_tests)
    set_tests_properties(guest_icu_language_tag PROPERTIES TIMEOUT 10)
    if(WIN32)
        set_tests_properties(guest_icu_language_tag PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceIcu>")
    endif()
endif()

if(BUILD_TESTING)
    add_test_executable(guest_icu_script_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/Script.cpp)
    target_include_directories(guest_icu_script_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_icu_script_tests PRIVATE libSceIcu libc)
    configure_windows_unwind(guest_icu_script_tests)
    add_test(NAME guest_icu_script COMMAND guest_icu_script_tests)
    set_tests_properties(guest_icu_script PROPERTIES TIMEOUT 20)
    if(WIN32)
        set_tests_properties(guest_icu_script PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceIcu>")
    endif()
endif()

if(BUILD_TESTING)
    add_test_executable(guest_icu_locale_core_tests ${LIBS_INCLUDE_DIR}/tests/GuestIcuLocaleCore.cpp)
    target_include_directories(guest_icu_locale_core_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_icu_locale_core_tests PRIVATE libSceIcu)
    configure_windows_unwind(guest_icu_locale_core_tests)
    add_test(NAME guest_icu_locale_core COMMAND guest_icu_locale_core_tests)
    set_tests_properties(guest_icu_locale_core PROPERTIES TIMEOUT 20)
    if(WIN32)
        set_tests_properties(guest_icu_locale_core PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceIcu>")
    endif()
endif()

if(BUILD_TESTING)
    add_test_executable(guest_icu_terminate_buffer_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/TerminateBuffer.cpp)
    target_include_directories(guest_icu_terminate_buffer_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_icu_terminate_buffer_tests PRIVATE libSceIcu)
    configure_windows_unwind(guest_icu_terminate_buffer_tests)
    add_test(NAME guest_icu_terminate_buffer COMMAND guest_icu_terminate_buffer_tests)
    set_tests_properties(guest_icu_terminate_buffer PROPERTIES TIMEOUT 20)
    if(WIN32)
        set_tests_properties(guest_icu_terminate_buffer PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceIcu>")
    endif()
endif()

if(BUILD_TESTING)
    add_test_executable(guest_icu_locale_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/Locale.cpp)
    target_include_directories(guest_icu_locale_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_icu_locale_tests PRIVATE libSceIcu)
    configure_windows_unwind(guest_icu_locale_tests)
    add_test(NAME guest_icu_locale COMMAND guest_icu_locale_tests)
    set_tests_properties(guest_icu_locale PROPERTIES TIMEOUT 10)
    if(WIN32)
        set_tests_properties(guest_icu_locale PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceIcu>")
    endif()
endif()

if(BUILD_TESTING)
    add_test_executable(guest_icu_version_extents_tests ${LIBS_INCLUDE_DIR}/tests/GuestIcuVersionExtents.cpp)
    target_include_directories(guest_icu_version_extents_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_icu_version_extents_tests PRIVATE libSceIcu libc)
    configure_windows_unwind(guest_icu_version_extents_tests)
    add_test(NAME guest_icu_version_extents COMMAND guest_icu_version_extents_tests)
    set_tests_properties(guest_icu_version_extents PROPERTIES TIMEOUT 20)
    if(WIN32)
        set_tests_properties(guest_icu_version_extents PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceIcu>")
    endif()
endif()

if(BUILD_TESTING)
    add_test_executable(guest_icu_character_digits_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/CharacterDigits.cpp)
    target_include_directories(guest_icu_character_digits_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_icu_character_digits_tests PRIVATE libSceIcu)
    configure_windows_unwind(guest_icu_character_digits_tests)
    add_test(NAME guest_icu_character_digits COMMAND guest_icu_character_digits_tests)
    set_tests_properties(guest_icu_character_digits PROPERTIES TIMEOUT 10)
    if(WIN32)
        set_tests_properties(guest_icu_character_digits PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceIcu>")
    endif()
endif()

if(BUILD_TESTING)
    add_test_executable(guest_icu_utf8_tests ${LIBS_INCLUDE_DIR}/tests/GuestIcuUtf8.cpp)
    target_include_directories(guest_icu_utf8_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_icu_utf8_tests PRIVATE libSceIcu libc icu_common)
    configure_windows_unwind(guest_icu_utf8_tests)
    add_test(NAME guest_icu_utf8 COMMAND guest_icu_utf8_tests)
    set_tests_properties(guest_icu_utf8 PROPERTIES TIMEOUT 20)
    if(WIN32)
        set_tests_properties(guest_icu_utf8 PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceIcu>")
    endif()
endif()

if(BUILD_TESTING)
    add_test_executable(guest_icu_cstring_tests ${LIBS_INCLUDE_DIR}/tests/GuestIcuCString.cpp)
    target_include_directories(guest_icu_cstring_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_icu_cstring_tests PRIVATE libSceIcu libc)
    configure_windows_unwind(guest_icu_cstring_tests)
    add_test(NAME guest_icu_cstring COMMAND guest_icu_cstring_tests)
    set_tests_properties(guest_icu_cstring PROPERTIES TIMEOUT 10)
    if(WIN32)
        set_tests_properties(guest_icu_cstring PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceIcu>")
    endif()
endif()

if(BUILD_TESTING)
    add_test_executable(guest_icu_converter_callbacks_tests ${LIBS_INCLUDE_DIR}/tests/GuestIcuConverterCallbacks.cpp)
    target_include_directories(guest_icu_converter_callbacks_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_icu_converter_callbacks_tests PRIVATE libSceIcu libc)
    configure_windows_unwind(guest_icu_converter_callbacks_tests)
    add_test(NAME guest_icu_converter_callbacks COMMAND guest_icu_converter_callbacks_tests)
    set_tests_properties(guest_icu_converter_callbacks PROPERTIES TIMEOUT 10)
    if(WIN32)
        set_tests_properties(guest_icu_converter_callbacks PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceIcu>")
    endif()
endif()

if(BUILD_TESTING)
    add_test_executable(guest_icu_version_tests ${LIBS_INCLUDE_DIR}/tests/GuestIcuVersion.cpp)
    target_include_directories(guest_icu_version_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_icu_version_tests PRIVATE libSceIcu libc)
    configure_windows_unwind(guest_icu_version_tests)
    add_test(NAME guest_icu_version COMMAND guest_icu_version_tests)
    set_tests_properties(guest_icu_version PROPERTIES TIMEOUT 10)
    add_test_executable(guest_icu_numeric_tests ${LIBS_INCLUDE_DIR}/tests/GuestIcuNumeric.cpp)
    target_include_directories(guest_icu_numeric_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_icu_numeric_tests PRIVATE libSceIcu libc)
    configure_windows_unwind(guest_icu_numeric_tests)
    add_test(NAME guest_icu_numeric COMMAND guest_icu_numeric_tests)
    set_tests_properties(guest_icu_numeric PROPERTIES TIMEOUT 10)
    add_test_executable(guest_icu_error_name_tests ${LIBS_INCLUDE_DIR}/tests/GuestIcuErrorName.cpp)
    target_include_directories(guest_icu_error_name_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_icu_error_name_tests PRIVATE libSceIcu libc)
    configure_windows_unwind(guest_icu_error_name_tests)
    add_test(NAME guest_icu_error_name COMMAND guest_icu_error_name_tests)
    set_tests_properties(guest_icu_error_name PROPERTIES TIMEOUT 10)
    add_test_executable(guest_icu_constrained_field_position_tests ${LIBS_INCLUDE_DIR}/tests/GuestIcuConstrainedFieldPosition.cpp)
    target_include_directories(guest_icu_constrained_field_position_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_icu_constrained_field_position_tests PRIVATE libSceIcu libc)
    configure_windows_unwind(guest_icu_constrained_field_position_tests)
    add_test(NAME guest_icu_constrained_field_position COMMAND guest_icu_constrained_field_position_tests)
    set_tests_properties(guest_icu_constrained_field_position PROPERTIES TIMEOUT 10)
    add_test_executable(guest_icu_set_tests ${LIBS_INCLUDE_DIR}/tests/GuestIcuSet.cpp)
    target_include_directories(guest_icu_set_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_icu_set_tests PRIVATE libSceIcu libc)
    configure_windows_unwind(guest_icu_set_tests)
    add_test(NAME guest_icu_set COMMAND guest_icu_set_tests)
    set_tests_properties(guest_icu_set PROPERTIES TIMEOUT 30)
    add_test_executable(guest_icu_enumeration_tests ${LIBS_INCLUDE_DIR}/tests/GuestIcuEnumeration.cpp)
    target_include_directories(guest_icu_enumeration_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_icu_enumeration_tests PRIVATE libSceIcu libc)
    configure_windows_unwind(guest_icu_enumeration_tests)
    add_test(NAME guest_icu_enumeration COMMAND guest_icu_enumeration_tests)
    set_tests_properties(guest_icu_enumeration PROPERTIES TIMEOUT 10)
    add_test_executable(guest_icu_iterator_tests ${LIBS_INCLUDE_DIR}/tests/GuestIcuIterator.cpp)
    target_include_directories(guest_icu_iterator_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_icu_iterator_tests PRIVATE libSceIcu libc)
    configure_windows_unwind(guest_icu_iterator_tests)
    add_test(NAME guest_icu_iterator COMMAND guest_icu_iterator_tests)
    set_tests_properties(guest_icu_iterator PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(guest_icu_version guest_icu_numeric guest_icu_error_name guest_icu_constrained_field_position guest_icu_set guest_icu_enumeration guest_icu_iterator PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceIcu>")
    endif()
endif()

if(BUILD_TESTING)
    add_test_executable(guest_icu_space_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/Space.cpp)
    target_include_directories(guest_icu_space_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_icu_space_tests PRIVATE libSceIcu)
    configure_windows_unwind(guest_icu_space_tests)
    add_test(NAME guest_icu_space COMMAND guest_icu_space_tests)
    set_tests_properties(guest_icu_space PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(guest_icu_space PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceIcu>")
    endif()
endif()

if(BUILD_TESTING)
    add_test_executable(guest_icu_unescape_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/Unescape.cpp)
    target_include_directories(guest_icu_unescape_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_icu_unescape_tests PRIVATE libSceIcu)
    configure_windows_unwind(guest_icu_unescape_tests)
    add_test(NAME guest_icu_unescape COMMAND guest_icu_unescape_tests)
    set_tests_properties(guest_icu_unescape PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(guest_icu_unescape PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceIcu>")
    endif()
endif()

if(BUILD_TESTING)
    add_test_executable(guest_icu_compare_iter_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/CompareIter.cpp)
    target_include_directories(guest_icu_compare_iter_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_icu_compare_iter_tests PRIVATE libSceIcu)
    configure_windows_unwind(guest_icu_compare_iter_tests)
    add_test(NAME guest_icu_compare_iter COMMAND guest_icu_compare_iter_tests)
    set_tests_properties(guest_icu_compare_iter PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(guest_icu_compare_iter PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceIcu>")
    endif()
endif()

if(BUILD_TESTING)
    add_test_executable(guest_icu_resource_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/Resource.cpp)
    target_include_directories(guest_icu_resource_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_icu_resource_tests PRIVATE libSceIcu)
    configure_windows_unwind(guest_icu_resource_tests)
    add_test(NAME guest_icu_resource COMMAND guest_icu_resource_tests)
    set_tests_properties(guest_icu_resource PROPERTIES TIMEOUT 10)
    if(WIN32)
        set_tests_properties(guest_icu_resource PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceIcu>")
    endif()
endif()

if(BUILD_TESTING)
    add_test_executable(guest_icu_tokenize_tests ${LIBS_INCLUDE_DIR}/tests/GuestIcuTokenize.cpp)
    target_include_directories(guest_icu_tokenize_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_icu_tokenize_tests PRIVATE libSceIcu libkernel libc)
    configure_windows_unwind(guest_icu_tokenize_tests)
    add_test(NAME guest_icu_tokenize COMMAND guest_icu_tokenize_tests)
    set_tests_properties(guest_icu_tokenize PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(guest_icu_tokenize PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceIcu>;PATH=path_list_prepend:$<TARGET_FILE_DIR:libkernel>")
    endif()
endif()

if(BUILD_TESTING)
    add_test_executable(guest_icu_comparison_tests ${LIBS_INCLUDE_DIR}/tests/GuestIcuComparisons.cpp)
    target_include_directories(guest_icu_comparison_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_icu_comparison_tests PRIVATE libSceIcu libc)
    configure_windows_unwind(guest_icu_comparison_tests)
    add_test(NAME guest_icu_comparisons COMMAND guest_icu_comparison_tests)
    set_tests_properties(guest_icu_comparisons PROPERTIES TIMEOUT 20)
    if(WIN32)
        set_tests_properties(guest_icu_comparisons PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceIcu>")
    endif()
endif()

if(BUILD_TESTING)
    add_test_executable(guest_icu_utf32_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/Utf32.cpp)
    target_include_directories(guest_icu_utf32_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_icu_utf32_tests PRIVATE libSceIcu)
    configure_windows_unwind(guest_icu_utf32_tests)
    add_test(NAME guest_icu_utf32 COMMAND guest_icu_utf32_tests)
    set_tests_properties(guest_icu_utf32 PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(guest_icu_utf32 PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceIcu>")
    endif()
endif()

if(BUILD_TESTING)
    add_test_executable(guest_icu_java_utf8_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/JavaUtf8.cpp)
    target_include_directories(guest_icu_java_utf8_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_icu_java_utf8_tests PRIVATE libSceIcu)
    configure_windows_unwind(guest_icu_java_utf8_tests)
    add_test(NAME guest_icu_java_utf8 COMMAND guest_icu_java_utf8_tests)
    set_tests_properties(guest_icu_java_utf8 PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(guest_icu_java_utf8 PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceIcu>")
    endif()
endif()

if(BUILD_TESTING)
    add_test_executable(guest_icu_compare_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/Compare.cpp)
    target_include_directories(guest_icu_compare_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_icu_compare_tests PRIVATE libSceIcu)
    configure_windows_unwind(guest_icu_compare_tests)
    add_test(NAME guest_icu_compare COMMAND guest_icu_compare_tests)
    set_tests_properties(guest_icu_compare PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(guest_icu_compare PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceIcu>")
    endif()
endif()

if(BUILD_TESTING)
    add_test_executable(guest_icu_search_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/Search.cpp)
    target_include_directories(guest_icu_search_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_icu_search_tests PRIVATE libSceIcu)
    configure_windows_unwind(guest_icu_search_tests)
    add_test(NAME guest_icu_search COMMAND guest_icu_search_tests)
    set_tests_properties(guest_icu_search PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(guest_icu_search PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceIcu>")
    endif()
endif()

if(BUILD_TESTING)
    add_test_executable(guest_icu_character_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/Character.cpp)
    target_include_directories(guest_icu_character_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_icu_character_tests PRIVATE libSceIcu)
    configure_windows_unwind(guest_icu_character_tests)
    add_test(NAME guest_icu_character COMMAND guest_icu_character_tests)
    set_tests_properties(guest_icu_character PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(guest_icu_character PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceIcu>")
    endif()
endif()

if(BUILD_TESTING)
    add_test_executable(guest_icu_configuration_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/Configuration.cpp)
    target_include_directories(guest_icu_configuration_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_icu_configuration_tests PRIVATE libSceIcu)
    configure_windows_unwind(guest_icu_configuration_tests)
    add_test(NAME guest_icu_configuration COMMAND guest_icu_configuration_tests)
    set_tests_properties(guest_icu_configuration PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(guest_icu_configuration PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceIcu>")
    endif()
endif()

if(BUILD_TESTING)
    add_test_executable(guest_icu_properties_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/Properties.cpp)
    target_include_directories(guest_icu_properties_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_icu_properties_tests PRIVATE libSceIcu libc)
    configure_windows_unwind(guest_icu_properties_tests)
    add_test(NAME guest_icu_properties COMMAND guest_icu_properties_tests)
    set_tests_properties(guest_icu_properties PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(guest_icu_properties PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceIcu>")
    endif()
endif()

if(BUILD_TESTING)
    add_test_executable(guest_icu_case_mapping_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/CaseMapping.cpp)
    target_include_directories(guest_icu_case_mapping_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_icu_case_mapping_tests PRIVATE libSceIcu libc)
    configure_windows_unwind(guest_icu_case_mapping_tests)
    add_test(NAME guest_icu_case_mapping COMMAND guest_icu_case_mapping_tests)
    set_tests_properties(guest_icu_case_mapping PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(guest_icu_case_mapping PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceIcu>")
    endif()
endif()

if(BUILD_TESTING)
    add_test_executable(guest_icu_unicode_data_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/UnicodeData.cpp)
    target_include_directories(guest_icu_unicode_data_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_compile_definitions(guest_icu_unicode_data_tests PRIVATE ANYPS5_ICU_UNICODE_DATA="${ICU_SOURCE_DIR}/data/unidata/UnicodeData.txt")
    target_link_libraries(guest_icu_unicode_data_tests PRIVATE libSceIcu libc)
    configure_windows_unwind(guest_icu_unicode_data_tests)
    add_test(NAME guest_icu_unicode_data COMMAND guest_icu_unicode_data_tests)
    set_tests_properties(guest_icu_unicode_data PROPERTIES TIMEOUT 60)
    if(WIN32)
        set_tests_properties(guest_icu_unicode_data PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceIcu>")
    endif()
endif()

if(BUILD_TESTING)
    add_test_executable(guest_icu_bidi_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/Bidi.cpp)
    target_include_directories(guest_icu_bidi_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_icu_bidi_tests PRIVATE libSceIcu libc)
    configure_windows_unwind(guest_icu_bidi_tests)
    add_test(NAME guest_icu_bidi COMMAND guest_icu_bidi_tests)
    set_tests_properties(guest_icu_bidi PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(guest_icu_bidi PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceIcu>")
    endif()
endif()

if(BUILD_TESTING)
    add_test_executable(guest_icu_charset_detection_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/CharsetDetection.cpp)
    target_include_directories(guest_icu_charset_detection_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_icu_charset_detection_tests PRIVATE libSceIcu libc)
    configure_windows_unwind(guest_icu_charset_detection_tests)
    add_test(NAME guest_icu_charset_detection COMMAND guest_icu_charset_detection_tests)
    set_tests_properties(guest_icu_charset_detection PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(guest_icu_charset_detection PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceIcu>")
    endif()
endif()

if(BUILD_TESTING)
    add_test_executable(guest_icu_normalization_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/Normalization.cpp)
    target_include_directories(guest_icu_normalization_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_icu_normalization_tests PRIVATE libSceIcu libc)
    configure_windows_unwind(guest_icu_normalization_tests)
    add_test(NAME guest_icu_normalization COMMAND guest_icu_normalization_tests)
    set_tests_properties(guest_icu_normalization PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(guest_icu_normalization PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceIcu>")
    endif()
endif()

if(BUILD_TESTING)
    add_test_executable(guest_icu_code_points_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/CodePoints.cpp)
    target_include_directories(guest_icu_code_points_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_icu_code_points_tests PRIVATE libSceIcu libc)
    configure_windows_unwind(guest_icu_code_points_tests)
    add_test(NAME guest_icu_code_points COMMAND guest_icu_code_points_tests)
    set_tests_properties(guest_icu_code_points PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(guest_icu_code_points PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceIcu>")
    endif()
endif()

target_sources(libSceIcu PRIVATE ConverterHelpers.cpp)

if(BUILD_TESTING)
    add_test_executable(guest_icu_converter_helpers_tests ${LIBS_INCLUDE_DIR}/tests/GuestIcuConverterHelpers.cpp)
    target_include_directories(guest_icu_converter_helpers_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_icu_converter_helpers_tests PRIVATE libSceIcu libc)
    configure_windows_unwind(guest_icu_converter_helpers_tests)
    add_test(NAME guest_icu_converter_helpers COMMAND guest_icu_converter_helpers_tests)
    set_tests_properties(guest_icu_converter_helpers PROPERTIES TIMEOUT 10)
    if(WIN32)
        set_tests_properties(guest_icu_converter_helpers PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceIcu>")
    endif()
    add_test_executable(guest_icu_copy_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/Copy.cpp)
    target_include_directories(guest_icu_copy_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_icu_copy_tests PRIVATE libSceIcu)
    configure_windows_unwind(guest_icu_copy_tests)
    add_test(NAME guest_icu_copy COMMAND guest_icu_copy_tests)
    set_tests_properties(guest_icu_copy PROPERTIES TIMEOUT 10)
    if(WIN32)
        set_tests_properties(guest_icu_copy PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceIcu>")
    endif()
    add_test_executable(guest_icu_copy_guest_memory_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/CopyGuestMemory.cpp)
    target_include_directories(guest_icu_copy_guest_memory_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_icu_copy_guest_memory_tests PRIVATE libSceIcu libkernel libc)
    configure_windows_unwind(guest_icu_copy_guest_memory_tests)
    add_test(NAME guest_icu_copy_guest_memory COMMAND guest_icu_copy_guest_memory_tests)
    set_tests_properties(guest_icu_copy_guest_memory PROPERTIES TIMEOUT 10)
    if(WIN32)
        set_tests_properties(guest_icu_copy_guest_memory PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceIcu>")
    endif()
endif()

if(BUILD_TESTING)
    add_test_executable(guest_icu_locale_map_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/LocaleMap.cpp)
    target_include_directories(guest_icu_locale_map_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_icu_locale_map_tests PRIVATE libSceIcu libc)
    configure_windows_unwind(guest_icu_locale_map_tests)
    add_test(NAME guest_icu_locale_map COMMAND guest_icu_locale_map_tests)
    set_tests_properties(guest_icu_locale_map PROPERTIES TIMEOUT 10)
    if(WIN32)
        set_tests_properties(guest_icu_locale_map PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceIcu>")
    endif()
endif()

set(ICU_RECOVERED_SOURCES
    Calendar.cpp
    Collator.cpp
    DateFormat.cpp
    DateIntervalFormat.cpp
    DateTimePatternGenerator.cpp
    FieldPositionIterator.cpp
    FormattedValue.cpp
    Idna.cpp
    ListFormatter.cpp
    LocaleDisplayNames.cpp
    NumberFormat.cpp
    NumberFormatter.cpp
    NumberingSystem.cpp
    PluralRules.cpp
    RelativeDateTimeFormatter.cpp
    ResourceBundle.cpp
    StringSearch.cpp
    UText.cpp)
target_sources(libSceIcu PRIVATE ${ICU_RECOVERED_SOURCES})

if(BUILD_TESTING)
    set(ICU_RECOVERED_TESTS
        tests/Calendar.cpp
        tests/Collator.cpp
        tests/DateFormat.cpp
        tests/DateIntervalFormat.cpp
        tests/DateTimePatternGenerator.cpp
        tests/FieldPositionIterator.cpp
        tests/FormattedValue.cpp
        tests/Idna.cpp
        tests/ListFormatter.cpp
        tests/LocaleDisplayNames.cpp
        tests/NumberFormat.cpp
        tests/NumberFormatter.cpp
        tests/NumberingSystem.cpp
        tests/PluralRules.cpp
        tests/RecoveredSignatures.cpp
        tests/RelativeDateTimeFormatter.cpp
        tests/ResourceBundle.cpp
        tests/StringSearch.cpp
        tests/UText.cpp)
    list(TRANSFORM ICU_RECOVERED_TESTS PREPEND ${CMAKE_CURRENT_SOURCE_DIR}/)
    add_test_executable(guest_icu_recovered_signatures_tests ${ICU_RECOVERED_TESTS})
    target_include_directories(guest_icu_recovered_signatures_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_icu_recovered_signatures_tests PRIVATE libSceIcu)
    configure_windows_unwind(guest_icu_recovered_signatures_tests)
    add_test(NAME guest_icu_recovered_signatures COMMAND guest_icu_recovered_signatures_tests)
    set_tests_properties(guest_icu_recovered_signatures PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(guest_icu_recovered_signatures PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceIcu>")
    endif()
endif()
target_sources(libSceIcu PRIVATE Hash.cpp)

if(BUILD_TESTING)
    add_test_executable(guest_icu_hash_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/Hash.cpp)
    target_include_directories(guest_icu_hash_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_icu_hash_tests PRIVATE libSceIcu)
    configure_windows_unwind(guest_icu_hash_tests)
    add_test(NAME guest_icu_hash COMMAND guest_icu_hash_tests)
    set_tests_properties(guest_icu_hash PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(guest_icu_hash PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceIcu>")
    endif()
endif()

target_sources(libSceIcu PRIVATE List.cpp)

if(BUILD_TESTING)
    add_test_executable(guest_icu_list_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/List.cpp)
    target_include_directories(guest_icu_list_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_icu_list_tests PRIVATE libSceIcu)
    configure_windows_unwind(guest_icu_list_tests)
    add_test(NAME guest_icu_list COMMAND guest_icu_list_tests)
    set_tests_properties(guest_icu_list PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(guest_icu_list PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceIcu>")
    endif()
endif()

target_sources(libSceIcu PRIVATE StringsEnumeration.cpp)

if(BUILD_TESTING)
    add_test_executable(guest_icu_strings_enumeration_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/StringsEnumeration.cpp)
    target_include_directories(guest_icu_strings_enumeration_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_icu_strings_enumeration_tests PRIVATE libSceIcu libc)
    configure_windows_unwind(guest_icu_strings_enumeration_tests)
    add_test(NAME guest_icu_strings_enumeration COMMAND guest_icu_strings_enumeration_tests)
    set_tests_properties(guest_icu_strings_enumeration PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(guest_icu_strings_enumeration PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceIcu>")
    endif()
endif()

target_sources(libSceIcu PRIVATE Trace.cpp)

if(BUILD_TESTING)
    add_test_executable(guest_icu_trace_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/Trace.cpp)
    target_include_directories(guest_icu_trace_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_icu_trace_tests PRIVATE libSceIcu)
    configure_windows_unwind(guest_icu_trace_tests)
    add_test(NAME guest_icu_trace COMMAND guest_icu_trace_tests)
    set_tests_properties(guest_icu_trace PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(guest_icu_trace PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceIcu>")
    endif()
endif()

target_sources(libSceIcu PRIVATE PlatformUtil.cpp)

if(BUILD_TESTING)
    add_test_executable(guest_icu_platformutil_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/PlatformUtil.cpp)
    target_include_directories(guest_icu_platformutil_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_icu_platformutil_tests PRIVATE libSceIcu)
    configure_windows_unwind(guest_icu_platformutil_tests)
    add_test(NAME guest_icu_platformutil COMMAND guest_icu_platformutil_tests)
    set_tests_properties(guest_icu_platformutil PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(guest_icu_platformutil PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceIcu>")
    endif()
endif()

target_sources(libSceIcu PRIVATE Cleanup.cpp)

if(BUILD_TESTING)
    add_test_executable(guest_icu_cleanup_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/Cleanup.cpp)
    target_include_directories(guest_icu_cleanup_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_icu_cleanup_tests PRIVATE libSceIcu)
    configure_windows_unwind(guest_icu_cleanup_tests)
    add_test(NAME guest_icu_cleanup COMMAND guest_icu_cleanup_tests)
    set_tests_properties(guest_icu_cleanup PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(guest_icu_cleanup PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceIcu>")
    endif()
endif()

target_sources(libSceIcu PRIVATE Terminate.cpp)

if(BUILD_TESTING)
    add_test_executable(guest_icu_terminate_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/Terminate.cpp)
    target_include_directories(guest_icu_terminate_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_icu_terminate_tests PRIVATE libSceIcu)
    configure_windows_unwind(guest_icu_terminate_tests)
    add_test(NAME guest_icu_terminate COMMAND guest_icu_terminate_tests)
    set_tests_properties(guest_icu_terminate PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(guest_icu_terminate PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceIcu>")
    endif()
endif()

target_sources(libSceIcu PRIVATE CodePointTrie.cpp)

if(BUILD_TESTING)
    add_test_executable(guest_icu_code_point_trie_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/CodePointTrie.cpp)
    target_include_directories(guest_icu_code_point_trie_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_icu_code_point_trie_tests PRIVATE libSceIcu)
    configure_windows_unwind(guest_icu_code_point_trie_tests)
    add_test(NAME guest_icu_code_point_trie COMMAND guest_icu_code_point_trie_tests)
    set_tests_properties(guest_icu_code_point_trie PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(guest_icu_code_point_trie PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceIcu>")
    endif()
endif()

target_sources(libSceIcu PRIVATE MutableCodePointTrie.cpp)

if(BUILD_TESTING)
    add_test_executable(guest_icu_mutable_code_point_trie_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/MutableCodePointTrie.cpp)
    target_include_directories(guest_icu_mutable_code_point_trie_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_icu_mutable_code_point_trie_tests PRIVATE libSceIcu)
    configure_windows_unwind(guest_icu_mutable_code_point_trie_tests)
    add_test(NAME guest_icu_mutable_code_point_trie COMMAND guest_icu_mutable_code_point_trie_tests)
    set_tests_properties(guest_icu_mutable_code_point_trie PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(guest_icu_mutable_code_point_trie PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceIcu>")
    endif()
endif()

target_sources(libSceIcu PRIVATE Trie2.cpp)

if(BUILD_TESTING)
    add_test_executable(guest_icu_trie2_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/Trie2.cpp)
    target_include_directories(guest_icu_trie2_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_icu_trie2_tests PRIVATE libSceIcu)
    configure_windows_unwind(guest_icu_trie2_tests)
    add_test(NAME guest_icu_trie2 COMMAND guest_icu_trie2_tests)
    set_tests_properties(guest_icu_trie2 PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(guest_icu_trie2 PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceIcu>")
    endif()
endif()

target_sources(libSceIcu PRIVATE Text.cpp)

if(BUILD_TESTING)
    add_test_executable(guest_icu_text_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/Text.cpp)
    target_include_directories(guest_icu_text_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_icu_text_tests PRIVATE libSceIcu)
    configure_windows_unwind(guest_icu_text_tests)
    add_test(NAME guest_icu_text COMMAND guest_icu_text_tests)
    set_tests_properties(guest_icu_text PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(guest_icu_text PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceIcu>")
    endif()
endif()

target_sources(libSceIcu PRIVATE Vector.cpp)

if(BUILD_TESTING)
    add_test_executable(guest_icu_vector_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/Vector.cpp)
    target_include_directories(guest_icu_vector_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_icu_vector_tests PRIVATE libSceIcu)
    configure_windows_unwind(guest_icu_vector_tests)
    add_test(NAME guest_icu_vector COMMAND guest_icu_vector_tests)
    set_tests_properties(guest_icu_vector PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(guest_icu_vector PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceIcu>")
    endif()
endif()

target_sources(libSceIcu PRIVATE Vector32.cpp)

if(BUILD_TESTING)
    add_test_executable(guest_icu_vector32_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/Vector32.cpp)
    target_include_directories(guest_icu_vector32_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_icu_vector32_tests PRIVATE libSceIcu)
    configure_windows_unwind(guest_icu_vector32_tests)
    add_test(NAME guest_icu_vector32 COMMAND guest_icu_vector32_tests)
    set_tests_properties(guest_icu_vector32 PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(guest_icu_vector32 PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceIcu>")
    endif()
endif()

target_sources(libSceIcu PRIVATE Edits.cpp)

if(BUILD_TESTING)
    add_test_executable(guest_icu_edits_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/Edits.cpp)
    target_include_directories(guest_icu_edits_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_icu_edits_tests PRIVATE libSceIcu)
    configure_windows_unwind(guest_icu_edits_tests)
    add_test(NAME guest_icu_edits COMMAND guest_icu_edits_tests)
    set_tests_properties(guest_icu_edits PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(guest_icu_edits PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceIcu>")
    endif()
endif()

target_sources(libSceIcu PRIVATE BytesTrie.cpp)

if(BUILD_TESTING)
    add_test_executable(guest_icu_bytes_trie_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/BytesTrie.cpp)
    target_include_directories(guest_icu_bytes_trie_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_icu_bytes_trie_tests PRIVATE libSceIcu)
    configure_windows_unwind(guest_icu_bytes_trie_tests)
    add_test(NAME guest_icu_bytes_trie COMMAND guest_icu_bytes_trie_tests)
    set_tests_properties(guest_icu_bytes_trie PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(guest_icu_bytes_trie PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceIcu>")
    endif()
endif()

target_sources(libSceIcu PRIVATE UCharsTrie.cpp)

if(BUILD_TESTING)
    add_test_executable(guest_icu_uchars_trie_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/UCharsTrie.cpp)
    target_include_directories(guest_icu_uchars_trie_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_icu_uchars_trie_tests PRIVATE libSceIcu)
    configure_windows_unwind(guest_icu_uchars_trie_tests)
    add_test(NAME guest_icu_uchars_trie COMMAND guest_icu_uchars_trie_tests)
    set_tests_properties(guest_icu_uchars_trie PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(guest_icu_uchars_trie PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceIcu>")
    endif()
endif()

target_sources(libSceIcu PRIVATE CharacterIterator.cpp)

if(BUILD_TESTING)
    add_test_executable(guest_icu_character_iterator_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/CharacterIterator.cpp)
    target_include_directories(guest_icu_character_iterator_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_icu_character_iterator_tests PRIVATE libSceIcu)
    configure_windows_unwind(guest_icu_character_iterator_tests)
    add_test(NAME guest_icu_character_iterator COMMAND guest_icu_character_iterator_tests)
    set_tests_properties(guest_icu_character_iterator PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(guest_icu_character_iterator PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceIcu>")
    endif()
endif()

target_sources(libSceIcu PRIVATE UnicodeString.cpp)

if(BUILD_TESTING)
    add_test_executable(guest_icu_unicode_string_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/UnicodeString.cpp)
    target_include_directories(guest_icu_unicode_string_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_icu_unicode_string_tests PRIVATE libSceIcu)
    configure_windows_unwind(guest_icu_unicode_string_tests)
    add_test(NAME guest_icu_unicode_string COMMAND guest_icu_unicode_string_tests)
    set_tests_properties(guest_icu_unicode_string PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(guest_icu_unicode_string PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceIcu>")
    endif()
endif()

target_sources(libSceIcu PRIVATE CharString.cpp)

if(BUILD_TESTING)
    add_test_executable(guest_icu_char_string_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/CharString.cpp)
    target_include_directories(guest_icu_char_string_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_icu_char_string_tests PRIVATE libSceIcu)
    configure_windows_unwind(guest_icu_char_string_tests)
    add_test(NAME guest_icu_char_string COMMAND guest_icu_char_string_tests)
    set_tests_properties(guest_icu_char_string PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(guest_icu_char_string PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceIcu>")
    endif()
endif()

target_sources(libSceIcu PRIVATE StringPiece.cpp)

if(BUILD_TESTING)
    add_test_executable(guest_icu_string_piece_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/StringPiece.cpp)
    target_include_directories(guest_icu_string_piece_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_icu_string_piece_tests PRIVATE libSceIcu)
    configure_windows_unwind(guest_icu_string_piece_tests)
    add_test(NAME guest_icu_string_piece COMMAND guest_icu_string_piece_tests)
    set_tests_properties(guest_icu_string_piece PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(guest_icu_string_piece PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceIcu>")
    endif()
endif()

target_sources(libSceIcu PRIVATE Appendable.cpp)

if(BUILD_TESTING)
    add_test_executable(guest_icu_appendable_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/Appendable.cpp)
    target_include_directories(guest_icu_appendable_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_icu_appendable_tests PRIVATE libSceIcu)
    configure_windows_unwind(guest_icu_appendable_tests)
    add_test(NAME guest_icu_appendable COMMAND guest_icu_appendable_tests)
    set_tests_properties(guest_icu_appendable PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(guest_icu_appendable PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceIcu>")
    endif()
endif()

target_sources(libSceIcu PRIVATE ByteSink.cpp)

if(BUILD_TESTING)
    add_test_executable(guest_icu_byte_sink_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/ByteSink.cpp)
    target_include_directories(guest_icu_byte_sink_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_icu_byte_sink_tests PRIVATE libSceIcu)
    configure_windows_unwind(guest_icu_byte_sink_tests)
    add_test(NAME guest_icu_byte_sink COMMAND guest_icu_byte_sink_tests)
    set_tests_properties(guest_icu_byte_sink PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(guest_icu_byte_sink PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceIcu>")
    endif()
endif()

target_sources(libSceIcu PRIVATE MaybeStackArray.cpp)

if(BUILD_TESTING)
    add_test_executable(guest_icu_maybe_stack_array_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/MaybeStackArray.cpp)
    target_include_directories(guest_icu_maybe_stack_array_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_icu_maybe_stack_array_tests PRIVATE libSceIcu)
    configure_windows_unwind(guest_icu_maybe_stack_array_tests)
    add_test(NAME guest_icu_maybe_stack_array COMMAND guest_icu_maybe_stack_array_tests)
    set_tests_properties(guest_icu_maybe_stack_array PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(guest_icu_maybe_stack_array PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceIcu>")
    endif()
endif()

target_sources(libSceIcu PRIVATE PatternProps.cpp)

if(BUILD_TESTING)
    add_test_executable(guest_icu_pattern_props_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/PatternProps.cpp)
    target_include_directories(guest_icu_pattern_props_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_icu_pattern_props_tests PRIVATE libSceIcu)
    configure_windows_unwind(guest_icu_pattern_props_tests)
    add_test(NAME guest_icu_pattern_props COMMAND guest_icu_pattern_props_tests)
    set_tests_properties(guest_icu_pattern_props PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(guest_icu_pattern_props PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceIcu>")
    endif()
endif()

target_sources(libSceIcu PRIVATE IcuUtility.cpp)

if(BUILD_TESTING)
    add_test_executable(guest_icu_icu_utility_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/IcuUtility.cpp)
    target_include_directories(guest_icu_icu_utility_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_icu_icu_utility_tests PRIVATE libSceIcu)
    configure_windows_unwind(guest_icu_icu_utility_tests)
    add_test(NAME guest_icu_icu_utility COMMAND guest_icu_icu_utility_tests)
    set_tests_properties(guest_icu_icu_utility PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(guest_icu_icu_utility PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceIcu>")
    endif()
endif()

target_sources(libSceIcu PRIVATE Object.cpp)

target_sources(libSceIcu PRIVATE ServiceNotifier.cpp)

if(BUILD_TESTING)
    add_test_executable(guest_icu_service_notifier_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/ServiceNotifier.cpp)
    target_include_directories(guest_icu_service_notifier_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_icu_service_notifier_tests PRIVATE libSceIcu)
    configure_windows_unwind(guest_icu_service_notifier_tests)
    add_test(NAME guest_icu_service_notifier COMMAND guest_icu_service_notifier_tests)
    set_tests_properties(guest_icu_service_notifier PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(guest_icu_service_notifier PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceIcu>")
    endif()
endif()

target_sources(libSceIcu PRIVATE ServiceKey.cpp)

if(BUILD_TESTING)
    add_test_executable(guest_icu_service_key_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/ServiceKey.cpp)
    target_include_directories(guest_icu_service_key_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_icu_service_key_tests PRIVATE libSceIcu)
    configure_windows_unwind(guest_icu_service_key_tests)
    add_test(NAME guest_icu_service_key COMMAND guest_icu_service_key_tests)
    set_tests_properties(guest_icu_service_key PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(guest_icu_service_key PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceIcu>")
    endif()
endif()

target_sources(libSceIcu PRIVATE ServiceFactory.cpp)

if(BUILD_TESTING)
    add_test_executable(guest_icu_service_factory_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/ServiceFactory.cpp)
    target_include_directories(guest_icu_service_factory_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_icu_service_factory_tests PRIVATE libSceIcu)
    configure_windows_unwind(guest_icu_service_factory_tests)
    add_test(NAME guest_icu_service_factory COMMAND guest_icu_service_factory_tests)
    set_tests_properties(guest_icu_service_factory PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(guest_icu_service_factory PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceIcu>")
    endif()
endif()

target_sources(libSceIcu PRIVATE Service.cpp)

if(BUILD_TESTING)
    add_test_executable(guest_icu_service_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/Service.cpp)
    target_include_directories(guest_icu_service_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_icu_service_tests PRIVATE libSceIcu)
    configure_windows_unwind(guest_icu_service_tests)
    add_test(NAME guest_icu_service COMMAND guest_icu_service_tests)
    set_tests_properties(guest_icu_service PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(guest_icu_service PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceIcu>")
    endif()
endif()

target_sources(libSceIcu PRIVATE ServiceLocale.cpp)

if(BUILD_TESTING)
    add_test_executable(guest_icu_service_locale_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/ServiceLocale.cpp)
    target_include_directories(guest_icu_service_locale_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_icu_service_locale_tests PRIVATE libSceIcu)
    configure_windows_unwind(guest_icu_service_locale_tests)
    add_test(NAME guest_icu_service_locale COMMAND guest_icu_service_locale_tests)
    set_tests_properties(guest_icu_service_locale PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(guest_icu_service_locale PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceIcu>")
    endif()
endif()

target_sources(libSceIcu PRIVATE ServiceStringPair.cpp)

if(BUILD_TESTING)
    add_test_executable(guest_icu_service_string_pair_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/ServiceStringPair.cpp)
    target_include_directories(guest_icu_service_string_pair_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_icu_service_string_pair_tests PRIVATE libSceIcu)
    configure_windows_unwind(guest_icu_service_string_pair_tests)
    add_test(NAME guest_icu_service_string_pair COMMAND guest_icu_service_string_pair_tests)
    set_tests_properties(guest_icu_service_string_pair PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(guest_icu_service_string_pair PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceIcu>")
    endif()
endif()

target_sources(libSceIcu PRIVATE UnicodeSet.cpp)

if(BUILD_TESTING)
    add_test_executable(guest_icu_unicode_set_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/UnicodeSet.cpp)
    target_include_directories(guest_icu_unicode_set_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_icu_unicode_set_tests PRIVATE libSceIcu)
    configure_windows_unwind(guest_icu_unicode_set_tests)
    add_test(NAME guest_icu_unicode_set COMMAND guest_icu_unicode_set_tests)
    set_tests_properties(guest_icu_unicode_set PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(guest_icu_unicode_set PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceIcu>")
    endif()
endif()

target_sources(libSceIcu PRIVATE UnicodeSetPattern.cpp)

if(BUILD_TESTING)
    add_test_executable(guest_icu_unicode_set_pattern_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/UnicodeSetPattern.cpp)
    target_include_directories(guest_icu_unicode_set_pattern_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_icu_unicode_set_pattern_tests PRIVATE libSceIcu)
    configure_windows_unwind(guest_icu_unicode_set_pattern_tests)
    add_test(NAME guest_icu_unicode_set_pattern COMMAND guest_icu_unicode_set_pattern_tests)
    set_tests_properties(guest_icu_unicode_set_pattern PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(guest_icu_unicode_set_pattern PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceIcu>")
    endif()
endif()

target_sources(libSceIcu PRIVATE UnicodeSetIterator.cpp)

if(BUILD_TESTING)
    add_test_executable(guest_icu_unicode_set_iterator_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/UnicodeSetIterator.cpp)
    target_include_directories(guest_icu_unicode_set_iterator_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_icu_unicode_set_iterator_tests PRIVATE libSceIcu)
    configure_windows_unwind(guest_icu_unicode_set_iterator_tests)
    add_test(NAME guest_icu_unicode_set_iterator COMMAND guest_icu_unicode_set_iterator_tests)
    set_tests_properties(guest_icu_unicode_set_iterator PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(guest_icu_unicode_set_iterator PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceIcu>")
    endif()
endif()

target_sources(libSceIcu PRIVATE ParsePosition.cpp)

if(BUILD_TESTING)
    add_test_executable(guest_icu_parse_position_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/ParsePosition.cpp)
    target_include_directories(guest_icu_parse_position_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_icu_parse_position_tests PRIVATE libSceIcu)
    configure_windows_unwind(guest_icu_parse_position_tests)
    add_test(NAME guest_icu_parse_position COMMAND guest_icu_parse_position_tests)
    set_tests_properties(guest_icu_parse_position PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(guest_icu_parse_position PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceIcu>")
    endif()
endif()

target_sources(libSceIcu PRIVATE UnicodeFilter.cpp)

if(BUILD_TESTING)
    add_test_executable(guest_icu_unicode_filter_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/UnicodeFilter.cpp)
    target_include_directories(guest_icu_unicode_filter_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_icu_unicode_filter_tests PRIVATE libSceIcu)
    configure_windows_unwind(guest_icu_unicode_filter_tests)
    add_test(NAME guest_icu_unicode_filter COMMAND guest_icu_unicode_filter_tests)
    set_tests_properties(guest_icu_unicode_filter PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(guest_icu_unicode_filter PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceIcu>")
    endif()
endif()

target_sources(libSceIcu PRIVATE LocaleId.cpp)

if(BUILD_TESTING)
    add_test_executable(guest_icu_locale_id_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/LocaleId.cpp)
    target_include_directories(guest_icu_locale_id_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_icu_locale_id_tests PRIVATE libSceIcu)
    configure_windows_unwind(guest_icu_locale_id_tests)
    add_test(NAME guest_icu_locale_id COMMAND guest_icu_locale_id_tests)
    set_tests_properties(guest_icu_locale_id PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(guest_icu_locale_id PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceIcu>")
    endif()
endif()

target_sources(libSceIcu PRIVATE LocaleObject.cpp)

if(BUILD_TESTING)
    add_test_executable(guest_icu_locale_object_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/LocaleObject.cpp)
    target_include_directories(guest_icu_locale_object_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_icu_locale_object_tests PRIVATE libSceIcu)
    configure_windows_unwind(guest_icu_locale_object_tests)
    add_test(NAME guest_icu_locale_object COMMAND guest_icu_locale_object_tests)
    set_tests_properties(guest_icu_locale_object PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(guest_icu_locale_object PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceIcu>")
    endif()
endif()

target_sources(libSceIcu PRIVATE LocaleBuilder.cpp)

if(BUILD_TESTING)
    add_test_executable(guest_icu_locale_builder_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/LocaleBuilder.cpp)
    target_include_directories(guest_icu_locale_builder_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_icu_locale_builder_tests PRIVATE libSceIcu)
    configure_windows_unwind(guest_icu_locale_builder_tests)
    add_test(NAME guest_icu_locale_builder COMMAND guest_icu_locale_builder_tests)
    set_tests_properties(guest_icu_locale_builder PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(guest_icu_locale_builder PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceIcu>")
    endif()
endif()

target_sources(libSceIcu PRIVATE CharacterIteratorObjects.cpp)

if(BUILD_TESTING)
    add_test_executable(guest_icu_character_iterator_objects_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/CharacterIteratorObjects.cpp)
    target_include_directories(guest_icu_character_iterator_objects_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_icu_character_iterator_objects_tests PRIVATE libSceIcu)
    configure_windows_unwind(guest_icu_character_iterator_objects_tests)
    add_test(NAME guest_icu_character_iterator_objects COMMAND guest_icu_character_iterator_objects_tests)
    set_tests_properties(guest_icu_character_iterator_objects PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(guest_icu_character_iterator_objects PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceIcu>")
    endif()
endif()

target_sources(libSceIcu PRIVATE StringCharacterIterator.cpp)

if(BUILD_TESTING)
    add_test_executable(guest_icu_string_character_iterator_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/StringCharacterIterator.cpp)
    target_include_directories(guest_icu_string_character_iterator_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_icu_string_character_iterator_tests PRIVATE libSceIcu)
    configure_windows_unwind(guest_icu_string_character_iterator_tests)
    add_test(NAME guest_icu_string_character_iterator COMMAND guest_icu_string_character_iterator_tests)
    set_tests_properties(guest_icu_string_character_iterator PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(guest_icu_string_character_iterator PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceIcu>")
    endif()
endif()

target_sources(libSceIcu PRIVATE StringEnumerationObjects.cpp)

if(BUILD_TESTING)
    add_test_executable(guest_icu_string_enumeration_objects_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/StringEnumerationObjects.cpp)
    target_include_directories(guest_icu_string_enumeration_objects_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_icu_string_enumeration_objects_tests PRIVATE libSceIcu libc)
    configure_windows_unwind(guest_icu_string_enumeration_objects_tests)
    add_test(NAME guest_icu_string_enumeration_objects COMMAND guest_icu_string_enumeration_objects_tests)
    set_tests_properties(guest_icu_string_enumeration_objects PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(guest_icu_string_enumeration_objects PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceIcu>")
    endif()
endif()


target_sources(libSceIcu PRIVATE VectorObjects.cpp)

if(BUILD_TESTING)
    add_test_executable(guest_icu_vector_objects_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/VectorObjects.cpp)
    target_include_directories(guest_icu_vector_objects_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_icu_vector_objects_tests PRIVATE libSceIcu)
    configure_windows_unwind(guest_icu_vector_objects_tests)
    add_test(NAME guest_icu_vector_objects COMMAND guest_icu_vector_objects_tests)
    set_tests_properties(guest_icu_vector_objects PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(guest_icu_vector_objects PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceIcu>")
    endif()
endif()


target_sources(libSceIcu PRIVATE ErrorCode.cpp)

if(BUILD_TESTING)
    add_test_executable(guest_icu_error_code_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/ErrorCode.cpp)
    target_include_directories(guest_icu_error_code_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_icu_error_code_tests PRIVATE libSceIcu)
    configure_windows_unwind(guest_icu_error_code_tests)
    add_test(NAME guest_icu_error_code COMMAND guest_icu_error_code_tests)
    set_tests_properties(guest_icu_error_code PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(guest_icu_error_code PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceIcu>")
    endif()
endif()


target_sources(libSceIcu PRIVATE ByteSinkUtil.cpp)

if(BUILD_TESTING)
    add_test_executable(guest_icu_byte_sink_util_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/ByteSinkUtil.cpp)
    target_include_directories(guest_icu_byte_sink_util_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_icu_byte_sink_util_tests PRIVATE libSceIcu)
    configure_windows_unwind(guest_icu_byte_sink_util_tests)
    add_test(NAME guest_icu_byte_sink_util COMMAND guest_icu_byte_sink_util_tests)
    set_tests_properties(guest_icu_byte_sink_util PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(guest_icu_byte_sink_util PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceIcu>")
    endif()
endif()


target_sources(libSceIcu PRIVATE BasicStringUtilities.cpp)

if(BUILD_TESTING)
    add_test_executable(guest_icu_basic_string_utilities_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/BasicStringUtilities.cpp)
    target_include_directories(guest_icu_basic_string_utilities_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_icu_basic_string_utilities_tests PRIVATE libSceIcu)
    configure_windows_unwind(guest_icu_basic_string_utilities_tests)
    add_test(NAME guest_icu_basic_string_utilities COMMAND guest_icu_basic_string_utilities_tests)
    set_tests_properties(guest_icu_basic_string_utilities PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(guest_icu_basic_string_utilities PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceIcu>")
    endif()
endif()


target_sources(libSceIcu PRIVATE UnicodeStringHash.cpp)

if(BUILD_TESTING)
    add_test_executable(guest_icu_unicode_string_hash_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/UnicodeStringHash.cpp)
    target_include_directories(guest_icu_unicode_string_hash_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_icu_unicode_string_hash_tests PRIVATE libSceIcu)
    configure_windows_unwind(guest_icu_unicode_string_hash_tests)
    add_test(NAME guest_icu_unicode_string_hash COMMAND guest_icu_unicode_string_hash_tests)
    set_tests_properties(guest_icu_unicode_string_hash PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(guest_icu_unicode_string_hash PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceIcu>")
    endif()
endif()


target_sources(libSceIcu PRIVATE UnicodeStringSink.cpp)

if(BUILD_TESTING)
    add_test_executable(guest_icu_unicode_string_sink_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/UnicodeStringSink.cpp)
    target_include_directories(guest_icu_unicode_string_sink_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_icu_unicode_string_sink_tests PRIVATE libSceIcu)
    configure_windows_unwind(guest_icu_unicode_string_sink_tests)
    add_test(NAME guest_icu_unicode_string_sink COMMAND guest_icu_unicode_string_sink_tests)
    set_tests_properties(guest_icu_unicode_string_sink PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(guest_icu_unicode_string_sink PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceIcu>")
    endif()
endif()



target_sources(libSceIcu PRIVATE TrieEnumeration.cpp)



if(BUILD_TESTING)

    add_test_executable(guest_icu_trie_enumeration_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/TrieEnumeration.cpp)

    target_include_directories(guest_icu_trie_enumeration_tests PRIVATE ${LIBS_INCLUDE_DIR})

    target_link_libraries(guest_icu_trie_enumeration_tests PRIVATE libSceIcu)

    configure_windows_unwind(guest_icu_trie_enumeration_tests)

    add_test(NAME guest_icu_trie_enumeration COMMAND guest_icu_trie_enumeration_tests)

    set_tests_properties(guest_icu_trie_enumeration PROPERTIES TIMEOUT 30)

    if(WIN32)

        set_tests_properties(guest_icu_trie_enumeration PROPERTIES ENVIRONMENT_MODIFICATION

            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceIcu>")

    endif()

endif()

target_sources(libSceIcu PRIVATE TrieBuilder.cpp)
target_sources(libSceIcu PRIVATE DataSwapper.cpp)

if(BUILD_TESTING)
    add_test_executable(guest_icu_trie_builder_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/TrieBuilder.cpp)
    target_include_directories(guest_icu_trie_builder_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_icu_trie_builder_tests PRIVATE libSceIcu)
    configure_windows_unwind(guest_icu_trie_builder_tests)
    add_test(NAME guest_icu_trie_builder COMMAND guest_icu_trie_builder_tests)
    set_tests_properties(guest_icu_trie_builder PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(guest_icu_trie_builder PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceIcu>")
    endif()

    add_test_executable(guest_icu_data_swapper_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/DataSwapper.cpp)
    target_include_directories(guest_icu_data_swapper_tests PRIVATE ${LIBS_INCLUDE_DIR})
    target_link_libraries(guest_icu_data_swapper_tests PRIVATE libSceIcu libc)
    configure_windows_unwind(guest_icu_data_swapper_tests)
    add_test(NAME guest_icu_data_swapper COMMAND guest_icu_data_swapper_tests)
    set_tests_properties(guest_icu_data_swapper PROPERTIES TIMEOUT 30)
    if(WIN32)
        set_tests_properties(guest_icu_data_swapper PROPERTIES ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:libSceIcu>")
    endif()
endif()

