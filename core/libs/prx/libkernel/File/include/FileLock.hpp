#ifndef CORE_LIBS_PRX_LIBKERNEL_FILE_FILELOCK_HPP
#define CORE_LIBS_PRX_LIBKERNEL_FILE_FILELOCK_HPP

namespace File {

int Flock(int fd, int operation);
void ForgetFileLock(int fd);

}

#endif
