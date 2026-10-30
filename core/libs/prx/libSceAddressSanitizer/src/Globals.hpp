#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace AsanRuntime::Globals {

struct SourceLocation {
    const char* filename;
    int lineNo;
    int columnNo;
};

struct Global {
    std::uintptr_t beg;
    std::uintptr_t size;
    std::uintptr_t sizeWithRedzone;
    const char* name;
    const char* moduleName;
    std::uintptr_t hasDynamicInit;
    const SourceLocation* location;
    std::uintptr_t odrIndicator;
};

constexpr std::size_t kMaxDescribed = 4;

void Register_nid_no_patch(Global* globals, std::uintptr_t count);
void Unregister_nid_no_patch(Global* globals, std::uintptr_t count);
void RegisterRange_nid_no_patch(std::uintptr_t* flag, void* start, void* stop);
void UnregisterRange_nid_no_patch(std::uintptr_t* flag, void* start, void* stop);
std::size_t ForAddress_nid_no_patch(std::uintptr_t address, Global* out);
std::string Describe_nid_no_patch(std::uintptr_t address, std::size_t accessSize);

}  // namespace AsanRuntime::Globals
