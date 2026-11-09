#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_SCANWIDEFIELD_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_SCANWIDEFIELD_HPP

#include <string>

namespace LibcDetail {

bool InWideScanset(const std::string& specifier, unsigned char byte);
void RequireNoPositionalWideScan(const char* format);

}

#endif
