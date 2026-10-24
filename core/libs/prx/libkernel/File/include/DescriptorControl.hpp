#ifndef CORE_LIBS_PRX_LIBKERNEL_FILE_DESCRIPTORCONTROL_HPP
#define CORE_LIBS_PRX_LIBKERNEL_FILE_DESCRIPTORCONTROL_HPP

#include <cstdint>

namespace File {

enum class FcntlArgument { None, Int, Pointer };

FcntlArgument FcntlArgumentOf(int command);
int Fcntl(int fd, int command, std::intptr_t argument);
bool IoctlRequestMalformed(std::uint64_t request);
int Ioctl(int fd, std::uint64_t request, void* argument);
int DuplicateFrom(int fd, int start);
void RememberDescriptor(int fd, int openFlags);
void RememberPipe(int fd);
void ShareDescriptor(int from, int to);
void ForgetDescriptor(int fd);
bool Appends(int fd);
bool IsVnode(int fd);
bool IsCharacterDevice(int fd);
bool LastDescriptorOfOpenFile(int fd);
bool OpenFileWrites(int fd);

}

extern "C" bool DescriptorHasNoVnode_nid_no_patch(int fd);

#endif
