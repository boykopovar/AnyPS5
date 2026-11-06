enable_language(C)

set(SQLITE_SOURCE_DIR ${CMAKE_SOURCE_DIR}/3rdparty/sqlite)
set(sqliteGen ${CMAKE_BINARY_DIR}/sqlite)
file(MAKE_DIRECTORY ${sqliteGen})

foreach(tool IN ITEMS lemon mkkeywordhash mksourceid)
    add_executable(sqlite_${tool} EXCLUDE_FROM_ALL ${SQLITE_SOURCE_DIR}/tool/${tool}.c)
    set_target_properties(sqlite_${tool} PROPERTIES OUTPUT_NAME ${tool} RUNTIME_OUTPUT_DIRECTORY ${sqliteGen})
endforeach()
add_executable(sqlite_jimsh EXCLUDE_FROM_ALL ${SQLITE_SOURCE_DIR}/autosetup/jimsh0.c)
if(WIN32)
    target_compile_definitions(sqlite_jimsh PRIVATE HAVE__FULLPATH)
else()
    target_compile_definitions(sqlite_jimsh PRIVATE HAVE_REALPATH)
endif()
set_target_properties(sqlite_jimsh PROPERTIES OUTPUT_NAME jimsh RUNTIME_OUTPUT_DIRECTORY ${sqliteGen})
set(sqliteTcl $<TARGET_FILE:sqlite_jimsh>)

add_custom_command(OUTPUT ${sqliteGen}/sqlite3.h
    COMMAND ${sqliteTcl} ${SQLITE_SOURCE_DIR}/tool/mksqlite3h.tcl ${SQLITE_SOURCE_DIR} -o ${sqliteGen}/sqlite3.h
    WORKING_DIRECTORY ${sqliteGen}
    DEPENDS sqlite_jimsh sqlite_mksourceid ${SQLITE_SOURCE_DIR}/src/sqlite.h.in ${SQLITE_SOURCE_DIR}/VERSION)
add_custom_command(OUTPUT ${sqliteGen}/parse.c ${sqliteGen}/parse.h
    COMMAND ${CMAKE_COMMAND} -E copy ${SQLITE_SOURCE_DIR}/src/parse.y ${SQLITE_SOURCE_DIR}/tool/lempar.c ${sqliteGen}
    COMMAND $<TARGET_FILE:sqlite_lemon> -S parse.y
    WORKING_DIRECTORY ${sqliteGen}
    DEPENDS sqlite_lemon ${SQLITE_SOURCE_DIR}/src/parse.y ${SQLITE_SOURCE_DIR}/tool/lempar.c)
add_custom_command(OUTPUT ${sqliteGen}/opcodes.h
    COMMAND ${CMAKE_COMMAND} -E cat ${sqliteGen}/parse.h ${SQLITE_SOURCE_DIR}/src/vdbe.c > ${sqliteGen}/opcodes.in
    COMMAND ${sqliteTcl} ${SQLITE_SOURCE_DIR}/tool/mkopcodeh.tcl < ${sqliteGen}/opcodes.in > ${sqliteGen}/opcodes.h
    WORKING_DIRECTORY ${sqliteGen}
    DEPENDS sqlite_jimsh ${sqliteGen}/parse.h ${SQLITE_SOURCE_DIR}/src/vdbe.c)
add_custom_command(OUTPUT ${sqliteGen}/opcodes.c
    COMMAND ${sqliteTcl} ${SQLITE_SOURCE_DIR}/tool/mkopcodec.tcl ${sqliteGen}/opcodes.h > ${sqliteGen}/opcodes.c
    WORKING_DIRECTORY ${sqliteGen}
    DEPENDS sqlite_jimsh ${sqliteGen}/opcodes.h)
add_custom_command(OUTPUT ${sqliteGen}/keywordhash.h
    COMMAND $<TARGET_FILE:sqlite_mkkeywordhash> > ${sqliteGen}/keywordhash.h
    WORKING_DIRECTORY ${sqliteGen}
    DEPENDS sqlite_mkkeywordhash)
add_custom_command(OUTPUT ${sqliteGen}/pragma.h
    COMMAND ${sqliteTcl} ${SQLITE_SOURCE_DIR}/tool/mkpragmatab.tcl ${sqliteGen}/pragma.h
    WORKING_DIRECTORY ${sqliteGen}
    DEPENDS sqlite_jimsh ${SQLITE_SOURCE_DIR}/tool/mkpragmatab.tcl)
add_custom_command(OUTPUT ${sqliteGen}/ctime.c
    COMMAND ${sqliteTcl} ${SQLITE_SOURCE_DIR}/tool/mkctimec.tcl ${sqliteGen}/ctime.c
    WORKING_DIRECTORY ${sqliteGen}
    DEPENDS sqlite_jimsh ${SQLITE_SOURCE_DIR}/tool/mkctimec.tcl)

set(sqliteSources)
foreach(name IN ITEMS alter analyze attach auth backup bitvec btmutex btree build callback carray complete date dbpage
        dbstat delete expr fault fkey func global hash insert json legacy loadext main malloc mem0 mem1 mem2 mem3
        mem5 memdb memjournal mutex mutex_noop mutex_unix mutex_w32 notify os os_kv os_unix os_win pager pcache
        pcache1 pragma prepare printf random resolve rowset select status table threads tokenize treeview trigger
        utf update upsert util vacuum vdbe vdbeapi vdbeaux vdbeblob vdbemem vdbesort vdbetrace vdbevtab vtab wal
        walker where wherecode whereexpr window)
    list(APPEND sqliteSources ${SQLITE_SOURCE_DIR}/src/${name}.c)
endforeach()
add_library(sqlite3 STATIC EXCLUDE_FROM_ALL ${sqliteSources} ${sqliteGen}/parse.c ${sqliteGen}/opcodes.c
    ${sqliteGen}/ctime.c ${sqliteGen}/sqlite3.h ${sqliteGen}/keywordhash.h ${sqliteGen}/pragma.h ${sqliteGen}/opcodes.h)
target_include_directories(sqlite3 PRIVATE ${sqliteGen} ${SQLITE_SOURCE_DIR}/src)
target_include_directories(sqlite3 SYSTEM INTERFACE ${sqliteGen})
target_compile_definitions(sqlite3 PRIVATE SQLITE_THREADSAFE=1 SQLITE_OMIT_LOAD_EXTENSION SQLITE_CORE)
set_target_properties(sqlite3 PROPERTIES C_STANDARD 99 POSITION_INDEPENDENT_CODE ON C_VISIBILITY_PRESET hidden)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(sqlite3 PRIVATE m pthread)
endif()
