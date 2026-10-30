#ifndef CORE_LIBS_PRX_LIBKERNEL_SYSTEM_HOSTCPU_HPP
#define CORE_LIBS_PRX_LIBKERNEL_SYSTEM_HOSTCPU_HPP

#include <cstdint>
#include <string>
#include <vector>

struct HostCpuRegisters {
    std::uint32_t Leaf1Ecx = 0;
    std::uint32_t Leaf1Edx = 0;
    std::uint32_t Leaf7Ebx = 0;
    std::uint32_t Leaf7Ecx = 0;
    std::uint32_t LeafDSub1Eax = 0;
    std::uint32_t Leaf80000001Ecx = 0;
    std::uint32_t Leaf80000008Ebx = 0;
    std::uint64_t Xcr0 = 0;
};

struct HostCpuFeature {
    const char* Name;
    bool Required;
    bool Present;
};

struct HostCpuText {
    char value[1024];
};

extern "C" {

HostCpuRegisters ReadHostCpuRegisters_nid_no_patch();
HostCpuText HostCpuModel_nid_no_patch();
HostCpuText MissingHostCpuFeatures_nid_no_patch(const HostCpuRegisters* registers);

}

HostCpuRegisters ReadHostCpuRegisters();
std::string HostCpuModel();
std::vector<HostCpuFeature> HostCpuFeatures(const HostCpuRegisters& registers);
std::vector<std::string> MissingHostCpuFeatures(const HostCpuRegisters& registers);
void CheckHostCpu(const HostCpuRegisters& registers, const std::string& model);

#endif
