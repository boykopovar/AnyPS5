add_library(libSceFreeTypeOt SHARED EXCLUDE_FROM_ALL Export.cpp ../libSceFreeType/src/OutlineBorders.cpp)
target_include_directories(libSceFreeTypeOt PRIVATE ${LIBS_INCLUDE_DIR})
configure_windows_unwind(libSceFreeTypeOt)
