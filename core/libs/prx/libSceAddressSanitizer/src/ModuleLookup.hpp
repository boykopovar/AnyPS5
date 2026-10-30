#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace AsanRuntime {

bool ModuleForAddress_nid_no_patch(std::uintptr_t address, std::string& name, std::uintptr_t& offset);
std::size_t CaptureBacktrace_nid_no_patch(std::uintptr_t* frames, std::size_t maxFrames, std::uintptr_t pc, std::uintptr_t framePointer);
std::uintptr_t HostProcessId_nid_no_patch();
const char* StripModuleName_nid_no_patch(const char* module);

}  // namespace AsanRuntime
