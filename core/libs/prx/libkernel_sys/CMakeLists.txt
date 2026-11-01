add_library(${LIB_NAME} SHARED EXCLUDE_FROM_ALL Export.cpp Export.Discovery.cpp Aio.cpp Cpuset.cpp ThreadQueries.cpp)
target_include_directories(${LIB_NAME} PRIVATE ${LIBS_INCLUDE_DIR})
target_sources(${LIB_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/AdditionalExports.cpp)
configure_windows_unwind(${LIB_NAME})

target_sources(${LIB_NAME} PRIVATE Uuid.cpp)
