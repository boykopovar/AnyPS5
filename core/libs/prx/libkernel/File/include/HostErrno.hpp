#ifndef CORE_LIBS_PRX_LIBKERNEL_FILE_HOSTERRNO_HPP
#define CORE_LIBS_PRX_LIBKERNEL_FILE_HOSTERRNO_HPP

#include <system_error>

namespace File {

int SceErrorFromHostErrno(int error);
bool IsPathLookupError(const std::error_code& error);

}

#endif
