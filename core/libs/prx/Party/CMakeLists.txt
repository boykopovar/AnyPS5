add_library(Party SHARED EXCLUDE_FROM_ALL Export.cpp)
target_include_directories(Party PRIVATE ${LIBS_INCLUDE_DIR})
configure_windows_unwind(Party)
