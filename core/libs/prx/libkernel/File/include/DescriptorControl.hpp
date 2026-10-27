#ifndef CORE_LIBS_PRX_LIBKERNEL_FILE_DESCRIPTORCONTROL_HPP
#define CORE_LIBS_PRX_LIBKERNEL_FILE_DESCRIPTORCONTROL_HPP

#include <cstdint>
#include <exception>

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
void SyncWrites(int fd);

class WriteSync {
public:
    explicit WriteSync(int fd) : _fd(fd), _exceptions(std::uncaught_exceptions()) {}
    ~WriteSync() noexcept(false) {
        if (std::uncaught_exceptions() == _exceptions) SyncWrites(_fd);
    }

private:
    int _fd;
    int _exceptions;
};
bool IsVnode(int fd);
bool IsCharacterDevice(int fd);
bool GrantsAccess(int fd, bool write);

}

extern "C" bool DescriptorHasNoVnode_nid_no_patch(int fd);

#endif
