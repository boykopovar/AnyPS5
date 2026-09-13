#ifndef CORE_LIBS_PRX_LIBKERNEL_FILE_PATH_HPP
#define CORE_LIBS_PRX_LIBKERNEL_FILE_PATH_HPP

#include <filesystem>


namespace File {

std::filesystem::path ResolvePath(const char* path);

}

#endif
