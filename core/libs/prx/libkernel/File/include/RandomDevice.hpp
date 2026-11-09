#ifndef CORE_LIBS_PRX_LIBKERNEL_FILE_RANDOMDEVICE_HPP
#define CORE_LIBS_PRX_LIBKERNEL_FILE_RANDOMDEVICE_HPP

#include <cstddef>
#include <string_view>

namespace File {

bool IsRandomDevicePath(std::string_view path);
int OpenRandomDevice();
bool IsRandomDevice(int fd);
bool ReadRandomDevice(int fd, void* buf, std::size_t nbytes);
void RememberRandomDevice(int fd);
void ForgetRandomDevice(int fd);

}

#endif
