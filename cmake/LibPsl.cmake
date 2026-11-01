function(add_bundled_libpsl)
    if(TARGET psl)
        return()
    endif()

    find_package(Python3 COMPONENTS Interpreter REQUIRED)

    set(PSL_SOURCE_DIR ${CMAKE_SOURCE_DIR}/3rdparty/libpsl)
    set(PSL_BINARY_DIR ${CMAKE_BINARY_DIR}/3rdparty/libpsl)
    file(MAKE_DIRECTORY ${PSL_BINARY_DIR})

    set(PSL_LIST_FILE ${PSL_SOURCE_DIR}/list/public_suffix_list.dat)
    set(PSL_DAFSA_HEADER ${PSL_BINARY_DIR}/suffixes_dafsa.h)

    add_custom_command(
        OUTPUT ${PSL_DAFSA_HEADER}
        COMMAND ${Python3_EXECUTABLE} ${PSL_SOURCE_DIR}/src/psl-make-dafsa --output-format=cxx+ ${PSL_LIST_FILE} ${PSL_DAFSA_HEADER}
        DEPENDS ${PSL_SOURCE_DIR}/src/psl-make-dafsa ${PSL_LIST_FILE}
        VERBATIM
    )

    set(LIBPSL_VERSION "0.21.5")
    set(LIBPSL_VERSION_MAJOR 0)
    set(LIBPSL_VERSION_MINOR 21)
    set(LIBPSL_VERSION_PATCH 5)
    set(LIBPSL_VERSION_NUMBER "0x001505")

    configure_file(${PSL_SOURCE_DIR}/include/libpsl.h.in ${PSL_BINARY_DIR}/libpsl.h @ONLY)

    add_library(psl STATIC EXCLUDE_FROM_ALL
        ${PSL_SOURCE_DIR}/src/psl.c
        ${PSL_SOURCE_DIR}/src/lookup_string_in_fixed_set.c
        ${PSL_DAFSA_HEADER}
    )

    target_include_directories(psl
        PUBLIC
            ${PSL_SOURCE_DIR}/include
            ${PSL_BINARY_DIR}
        PRIVATE
            ${PSL_SOURCE_DIR}/src
    )

    target_compile_definitions(psl
        PRIVATE
            PACKAGE_VERSION="0.21.5"
            BUILDING_PSL
            ENABLE_BUILTIN
            PSL_DISTFILE="public_suffix_list.dat"
    )

    if(WIN32)
        target_link_libraries(psl PRIVATE ws2_32)
    endif()

    set_target_properties(psl PROPERTIES
        POSITION_INDEPENDENT_CODE ON
        C_VISIBILITY_PRESET hidden
    )
    if(MINGW OR CMAKE_SYSTEM_NAME STREQUAL "Linux")
        target_link_options(psl INTERFACE LINKER:--exclude-libs,libpsl.a)
    endif()
endfunction()

add_bundled_libpsl()
