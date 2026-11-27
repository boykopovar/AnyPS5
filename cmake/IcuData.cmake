include(ExternalProject)
if(NOT Python3_EXECUTABLE)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
endif()
set(ANYPS5_ICU_HOST_C_COMPILER "" CACHE FILEPATH "Native C compiler for ICU data tools when cross compiling")
set(ANYPS5_ICU_HOST_CXX_COMPILER "" CACHE FILEPATH "Native C++ compiler for ICU data tools when cross compiling")
set(icu_host_compilers)
if(CMAKE_CROSSCOMPILING)
    if(NOT ANYPS5_ICU_HOST_C_COMPILER OR NOT ANYPS5_ICU_HOST_CXX_COMPILER)
        message(FATAL_ERROR "Set ANYPS5_ICU_HOST_C_COMPILER and ANYPS5_ICU_HOST_CXX_COMPILER to native compilers")
    endif()
    list(APPEND icu_host_compilers
        -DCMAKE_C_COMPILER=${ANYPS5_ICU_HOST_C_COMPILER}
        -DCMAKE_CXX_COMPILER=${ANYPS5_ICU_HOST_CXX_COMPILER})
else()
    list(APPEND icu_host_compilers -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER} -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER})
endif()
set(icu_host_dir ${CMAKE_BINARY_DIR}/icu-host)
string(REPLACE ";" "|" icu_host_c_launcher "${CMAKE_C_COMPILER_LAUNCHER}")
string(REPLACE ";" "|" icu_host_cxx_launcher "${CMAKE_CXX_COMPILER_LAUNCHER}")
set(icu_host_outputs)
foreach(tool genbrk gendict genrb gencnval icupkg genccode)
    if(CMAKE_HOST_WIN32)
        set(icu_host_${tool} ${icu_host_dir}/bin/${tool}/host/${tool}.exe)
    else()
        set(icu_host_${tool} ${icu_host_dir}/bin/${tool})
    endif()
    list(APPEND icu_host_outputs ${icu_host_${tool}})
endforeach()
ExternalProject_Add(icu_host_tools
    LIST_SEPARATOR |
    SOURCE_DIR ${CMAKE_SOURCE_DIR}/cmake/icu-host
    BINARY_DIR ${icu_host_dir}
    CMAKE_ARGS ${icu_host_compilers}
        -DCMAKE_TOOLCHAIN_FILE:FILEPATH=
        -DICU_SOURCE_DIR=${ICU_SOURCE_DIR}
        -DCMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}
        -DCMAKE_BUILD_TYPE=Release
        "-DCMAKE_C_COMPILER_LAUNCHER:STRING=${icu_host_c_launcher}"
        "-DCMAKE_CXX_COMPILER_LAUNCHER:STRING=${icu_host_cxx_launcher}"
    BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> --config Release --parallel 6
    INSTALL_COMMAND ""
    BUILD_ALWAYS TRUE
    BUILD_BYPRODUCTS ${icu_host_outputs}
    EXCLUDE_FROM_ALL TRUE)
set(icu_data_dir ${CMAKE_BINARY_DIR}/icu-data)
file(MAKE_DIRECTORY ${icu_data_dir})
file(RELATIVE_PATH icu_data_source ${icu_data_dir} ${ICU_SOURCE_DIR}/data)
file(RELATIVE_PATH icu_data_tools ${icu_data_dir} ${icu_host_dir}/bin)
foreach(path IN ITEMS "${icu_data_source}" "${icu_data_tools}")
    if(path MATCHES " ")
        message(FATAL_ERROR "ICU's data builder passes ${path} to a shell without quoting it; use a build directory whose path to ${ICU_SOURCE_DIR} has no spaces")
    endif()
endforeach()
file(GLOB_RECURSE icu_data_inputs CONFIGURE_DEPENDS ${ICU_SOURCE_DIR}/data/* ${ICU_SOURCE_DIR}/python/*.py)
if(CMAKE_HOST_WIN32)
    set(icu_data_mode windows-exec)
else()
    set(icu_data_mode unix-exec)
endif()
add_custom_command(OUTPUT ${icu_data_dir}/icudt78l_dat.c
    COMMAND ${CMAKE_COMMAND} -E make_directory ${icu_data_dir}/icudt78l ${icu_data_dir}/tmp
    COMMAND ${CMAKE_COMMAND} -E time ${Python3_EXECUTABLE} -B ${CMAKE_SOURCE_DIR}/cmake/IcuDataBuilder.py
        ${ICU_SOURCE_DIR}/python
        --mode=${icu_data_mode} --tool_cfg=host --tool_dir=${icu_data_tools}
        --src_dir=${icu_data_source} --out_dir=./icudt78l --tmp_dir=./tmp
        --filter_file=${CMAKE_SOURCE_DIR}/cmake/IcuDataFilter.json --verbose
    COMMAND ${icu_host_icupkg} -tl -a ${icu_data_dir}/tmp/icudata.lst
        -s ${icu_data_dir}/icudt78l new ${icu_data_dir}/icudt78l.dat
    COMMAND ${icu_host_genccode} -e icudt78 -d ${icu_data_dir} ${icu_data_dir}/icudt78l.dat
    DEPENDS icu_host_tools ${icu_host_outputs} ${icu_data_inputs} ${CMAKE_SOURCE_DIR}/cmake/IcuDataBuilder.py
        ${CMAKE_SOURCE_DIR}/cmake/IcuDataFilter.json
    WORKING_DIRECTORY ${icu_data_dir}
    VERBATIM)
get_target_property(icu_common_sources icu_common SOURCES)
list(REMOVE_ITEM icu_common_sources ${ICU_SOURCE_DIR}/stubdata/stubdata.cpp ${ICU_NORMALIZATION_SOURCE})
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${ICU_SOURCE_DIR}/common/sources.txt)
file(STRINGS ${ICU_SOURCE_DIR}/common/sources.txt icu_all_common)
list(TRANSFORM icu_all_common PREPEND ${ICU_SOURCE_DIR}/common/)
list(APPEND icu_common_sources ${icu_all_common})
list(REMOVE_DUPLICATES icu_common_sources)
set_property(TARGET icu_common PROPERTY SOURCES ${icu_common_sources})
set_property(TARGET icu_common PROPERTY C_VISIBILITY_PRESET hidden)
target_sources(icu_common PRIVATE ${icu_data_dir}/icudt78l_dat.c)
find_package(Threads REQUIRED)
target_link_libraries(icu_common PRIVATE Threads::Threads ${CMAKE_DL_LIBS})
