#include "prx/libScePosixForWebKit/include/Api.hpp"
#include "prx/libkernel_sys/include/Api.hpp"
#include "prx/libSceLibcInternal/include/Api.hpp"

extern "C" {

int APS5_VABI __wrap_fchown(int descriptor, std::uint32_t owner, std::uint32_t group) {
    return fchown_nid_postfix(descriptor, owner, group);
}

int APS5_VABI __wrap_link(const char* existing, const char* created) {
    return link_nid_postfix(existing, created);
}

std::int64_t APS5_VABI __wrap_readlink(const char* path, char* buffer, std::size_t size) {
    return readlink_nid_postfix(path, buffer, size);
}

int APS5_VABI __wrap_statvfs(const char* path, LibcStatvfs* status) {
    return statvfs_nid_postfix(path, status);
}

int APS5_VABI __wrap_symlink(const char* target, const char* created) {
    return symlink_nid_postfix(target, created);
}

}
