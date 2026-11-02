#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_SCANWIDEFIELD_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_SCANWIDEFIELD_HPP

#include <cstddef>
#include <string>

namespace LibcDetail {

std::size_t ReadWideScanCharacter(const char* text, char32_t* point);
bool InWideScanset(const std::string& specifier, char32_t point);
void RequireNoPositionalWideScan(const char* format);

}

#endif
