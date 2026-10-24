enable_language(C)

set(OGG_DIR ${CMAKE_SOURCE_DIR}/3rdparty/ogg)
set(VORBIS_DIR ${CMAKE_SOURCE_DIR}/3rdparty/vorbis)
set(OGG_GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR}/ogg/include)

include(CheckIncludeFiles)
check_include_files(inttypes.h INCLUDE_INTTYPES_H)
check_include_files(stdint.h INCLUDE_STDINT_H)
check_include_files(sys/types.h INCLUDE_SYS_TYPES_H)
set(SIZE16 int16_t)
set(USIZE16 uint16_t)
set(SIZE32 int32_t)
set(USIZE32 uint32_t)
set(SIZE64 int64_t)
set(USIZE64 uint64_t)
configure_file(${OGG_DIR}/include/ogg/config_types.h.in ${OGG_GENERATED_DIR}/ogg/config_types.h @ONLY)

add_library(ogg STATIC EXCLUDE_FROM_ALL
        ${OGG_DIR}/src/bitwise.c
        ${OGG_DIR}/src/framing.c
)
target_include_directories(ogg PUBLIC ${OGG_DIR}/include ${OGG_GENERATED_DIR})

add_library(vorbis STATIC EXCLUDE_FROM_ALL
        ${VORBIS_DIR}/lib/analysis.c
        ${VORBIS_DIR}/lib/bitrate.c
        ${VORBIS_DIR}/lib/block.c
        ${VORBIS_DIR}/lib/codebook.c
        ${VORBIS_DIR}/lib/envelope.c
        ${VORBIS_DIR}/lib/floor0.c
        ${VORBIS_DIR}/lib/floor1.c
        ${VORBIS_DIR}/lib/info.c
        ${VORBIS_DIR}/lib/lookup.c
        ${VORBIS_DIR}/lib/lpc.c
        ${VORBIS_DIR}/lib/lsp.c
        ${VORBIS_DIR}/lib/mapping0.c
        ${VORBIS_DIR}/lib/mdct.c
        ${VORBIS_DIR}/lib/psy.c
        ${VORBIS_DIR}/lib/registry.c
        ${VORBIS_DIR}/lib/res0.c
        ${VORBIS_DIR}/lib/sharedbook.c
        ${VORBIS_DIR}/lib/smallft.c
        ${VORBIS_DIR}/lib/synthesis.c
        ${VORBIS_DIR}/lib/window.c
)
target_include_directories(vorbis PUBLIC ${VORBIS_DIR}/include PRIVATE ${VORBIS_DIR}/lib)
target_link_libraries(vorbis PUBLIC ogg)
if(NOT WIN32)
    target_link_libraries(vorbis PRIVATE m)
endif()

add_library(vorbisfile STATIC EXCLUDE_FROM_ALL ${VORBIS_DIR}/lib/vorbisfile.c)
target_include_directories(vorbisfile PRIVATE ${VORBIS_DIR}/lib)
target_link_libraries(vorbisfile PUBLIC vorbis)
set_target_properties(ogg vorbis vorbisfile PROPERTIES POSITION_INDEPENDENT_CODE ON C_VISIBILITY_PRESET hidden)
if(MINGW)
    foreach(library IN ITEMS ogg vorbis vorbisfile)
        target_compile_options(${library} PRIVATE -fexceptions)
    endforeach()
endif()

add_library(libSceVorbisDec SHARED EXCLUDE_FROM_ALL Export.cpp)
target_include_directories(libSceVorbisDec PRIVATE ${LIBS_INCLUDE_DIR})
target_compile_definitions(libSceVorbisDec PRIVATE OV_EXCLUDE_STATIC_CALLBACKS)
target_link_libraries(libSceVorbisDec PRIVATE vorbisfile vorbis ogg)
if(MINGW OR CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_options(libSceVorbisDec PRIVATE LINKER:--exclude-libs,ALL)
endif()
configure_windows_unwind(libSceVorbisDec)

if(BUILD_TESTING)
    add_library(vorbisenc STATIC EXCLUDE_FROM_ALL ${VORBIS_DIR}/lib/vorbisenc.c)
    target_include_directories(vorbisenc PRIVATE ${VORBIS_DIR}/lib)
    target_link_libraries(vorbisenc PUBLIC vorbis)
    set_target_properties(vorbisenc PROPERTIES POSITION_INDEPENDENT_CODE ON C_VISIBILITY_PRESET hidden)
endif()
