#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_FILELOCK_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_FILELOCK_HPP

#ifdef _WIN32
extern "C" {
int GuestFileLock_nid_no_patch(int fd, int operation);
void GuestFileLockShare_nid_no_patch(int from, int to);
void GuestFileLockForget_nid_no_patch(int fd);
}
#endif

#endif
