#ifndef CORE_LIBS_PRX_LIBSCESYSTEMSERVICE_LOADEXEC_HPP
#define CORE_LIBS_PRX_LIBSCESYSTEMSERVICE_LOADEXEC_HPP

#include <string>
#include <vector>

namespace SystemService {

[[noreturn]] void RestartProcess(const std::vector<std::string>& arguments);

}  // namespace SystemService

#endif
