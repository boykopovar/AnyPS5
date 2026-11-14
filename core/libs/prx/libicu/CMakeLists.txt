add_library(libicu SHARED EXCLUDE_FROM_ALL Export.cpp)
target_include_directories(libicu PRIVATE ${LIBS_INCLUDE_DIR})
configure_windows_unwind(libicu)
