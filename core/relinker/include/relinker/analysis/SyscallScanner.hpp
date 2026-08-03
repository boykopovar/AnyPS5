#ifndef RELINKER_ANALYSIS_SYSCALLSCANNER_HPP
#define RELINKER_ANALYSIS_SYSCALLSCANNER_HPP

#include <relinker/domain/ISyscallScanner.hpp>
#include <memory>

namespace Relinker {

std::unique_ptr<ISyscallScanner> MakeSyscallScanner();

}

#endif
