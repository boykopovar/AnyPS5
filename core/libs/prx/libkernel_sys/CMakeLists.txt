add_library(${LIB_NAME} SHARED EXCLUDE_FROM_ALL Export.cpp Export.Discovery.cpp Aio.cpp ThreadQueries.cpp)
target_include_directories(${LIB_NAME} PRIVATE ${LIBS_INCLUDE_DIR})
configure_windows_unwind(${LIB_NAME})
