add_library(libcohtml SHARED EXCLUDE_FROM_ALL
    ${CMAKE_CURRENT_SOURCE_DIR}/Export.cpp
)
target_include_directories(libcohtml PRIVATE ${LIBS_INCLUDE_DIR})
set_target_properties(libcohtml PROPERTIES CXX_EXTENSIONS OFF)
configure_windows_unwind(libcohtml)

set(LIBCOHTML_PATCHED ${PATCHED_DIR}/libcohtml.prx)
set(LIBCOHTML_PROSPERO_PATCHED ${PATCHED_DIR}/libcohtml.Prospero.prx)
add_custom_command(
        OUTPUT ${LIBCOHTML_PROSPERO_PATCHED}
        COMMAND ${CMAKE_COMMAND} -E copy ${LIBCOHTML_PATCHED} ${LIBCOHTML_PROSPERO_PATCHED}
        DEPENDS ${LIBCOHTML_PATCHED}
        COMMENT "Providing libcohtml.Prospero.prx for DT_NEEDED"
        VERBATIM
)
add_custom_target(libcohtml_prospero DEPENDS ${LIBCOHTML_PROSPERO_PATCHED})
list(APPEND PATCHED_OUTPUTS ${LIBCOHTML_PROSPERO_PATCHED})
set(PATCHED_OUTPUTS ${PATCHED_OUTPUTS} PARENT_SCOPE)
