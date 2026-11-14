#ifndef RELINKER_ANALYSIS_SYSCALLSCANNER_HPP
#define RELINKER_ANALYSIS_SYSCALLSCANNER_HPP

#include <relinker/domain/ISyscallScanner.hpp>
#include <memory>
#include <domain/IProgress.hpp>

namespace Relinker {

std::unique_ptr<ISyscallScanner> MakeSyscallScanner(Domain::IProgress& progress, bool trapSdkStubs = false);
std::unique_ptr<ISyscallScanner> MakeNullSyscallScanner();
void ValidateSyscallRoutes(const std::vector<std::uint8_t>& bytes, const std::vector<ProgramHeader>& headers, const std::vector<RoutedSyscallStub>& routes, Domain::IProgress& progress, std::string_view input = {});

}

#endif
