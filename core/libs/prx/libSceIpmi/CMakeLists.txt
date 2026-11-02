add_library(libSceIpmi SHARED EXCLUDE_FROM_ALL Export.cpp PendingExports.cpp)
target_include_directories(libSceIpmi PRIVATE ${LIBS_INCLUDE_DIR})
configure_windows_unwind(libSceIpmi)
