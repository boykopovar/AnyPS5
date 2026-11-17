add_compile_definitions(curlx_nonblock=CurlSocketNonblock_nid_no_patch)
function(configureCurlSocketMode)
    set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/lib/nonblock.c"
        DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/lib"
        PROPERTIES COMPILE_OPTIONS "-Ucurlx_nonblock;-Dcurlx_nonblock=CurlHostNonblock_nid_no_patch")
endfunction()
cmake_language(DEFER CALL configureCurlSocketMode)
if(WIN32)
    add_compile_definitions(FD_SETSIZE=1024)
endif()
