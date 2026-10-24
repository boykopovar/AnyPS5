add_library(libSceGLSlimVSH SHARED EXCLUDE_FROM_ALL Export.cpp)
target_include_directories(libSceGLSlimVSH PRIVATE ${LIBS_INCLUDE_DIR} ${PROJECT_SOURCE_DIR}/3rdparty/SDL2/include)
configure_windows_unwind(libSceGLSlimVSH)
