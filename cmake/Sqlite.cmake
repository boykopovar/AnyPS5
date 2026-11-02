function(add_bundled_sqlite)
    if(TARGET sqlite3)
        return()
    endif()

    set(SQLITE_SOURCE_DIR ${CMAKE_SOURCE_DIR}/3rdparty/sqlite)

    add_library(sqlite3 STATIC EXCLUDE_FROM_ALL ${SQLITE_SOURCE_DIR}/sqlite3.c)
    target_include_directories(sqlite3 PUBLIC ${SQLITE_SOURCE_DIR})
    target_compile_definitions(sqlite3
        PRIVATE
            SQLITE_THREADSAFE=1
            SQLITE_OMIT_LOAD_EXTENSION
    )
    set_target_properties(sqlite3 PROPERTIES
        POSITION_INDEPENDENT_CODE ON
        C_VISIBILITY_PRESET hidden
    )
    if(UNIX)
        target_link_libraries(sqlite3 PUBLIC m)
    endif()
    if(MINGW OR CMAKE_SYSTEM_NAME STREQUAL "Linux")
        target_link_options(sqlite3 INTERFACE LINKER:--exclude-libs,libsqlite3.a)
    endif()
endfunction()

add_bundled_sqlite()
