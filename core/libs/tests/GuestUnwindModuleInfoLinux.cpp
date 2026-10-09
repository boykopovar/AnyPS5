#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <filesystem>
#include <link.h>
#include <string>

extern "C" int APS5_VABI sceKernelGetModuleInfoForUnwind(uint64_t addr, int flags, ModuleInfoForUnwind* info);

static void Require(bool value) { if (!value) std::abort(); }

namespace {

struct Image {
    std::uint64_t address = 0;
    std::string name;
    std::uint64_t firstLoad = 0;
    std::uint64_t firstSize = 0;
    std::uint64_t frameHeader = 0;
    bool found = false;
};

Image Find(std::uint64_t address) {
    Image image;
    image.address = address;
    dl_iterate_phdr([](dl_phdr_info* info, std::size_t, void* data) {
        auto& image = *static_cast<Image*>(data);
        const ElfW(Phdr)* first = nullptr;
        const ElfW(Phdr)* frames = nullptr;
        bool contains = false;
        for (ElfW(Half) index = 0; index < info->dlpi_phnum; ++index) {
            const auto& header = info->dlpi_phdr[index];
            const auto start = info->dlpi_addr + header.p_vaddr;
            if (header.p_type == PT_LOAD && first == nullptr) first = &header;
            if (header.p_type == PT_LOAD && image.address >= start && image.address - start < header.p_memsz) contains = true;
            if (header.p_type == PT_GNU_EH_FRAME) frames = &header;
        }
        if (!contains) return 0;
        image.name = info->dlpi_name != nullptr ? info->dlpi_name : "";
        image.firstLoad = info->dlpi_addr + first->p_vaddr;
        image.firstSize = first->p_memsz;
        image.frameHeader = frames != nullptr ? info->dlpi_addr + frames->p_vaddr : 0;
        image.found = true;
        return 1;
    }, &image);
    Require(image.found);
    return image;
}

std::uint64_t FirstLoadOf(const std::string& suffix) {
    std::pair<std::string, std::uint64_t> search{suffix, 0};
    dl_iterate_phdr([](dl_phdr_info* info, std::size_t, void* data) {
        auto& search = *static_cast<std::pair<std::string, std::uint64_t>*>(data);
        const std::string name = info->dlpi_name != nullptr ? info->dlpi_name : "";
        if (!name.ends_with(search.first)) return 0;
        for (ElfW(Half) index = 0; index < info->dlpi_phnum; ++index) {
            if (info->dlpi_phdr[index].p_type != PT_LOAD) continue;
            search.second = info->dlpi_addr + info->dlpi_phdr[index].p_vaddr;
            return 1;
        }
        return 0;
    }, &search);
    Require(search.second != 0);
    return search.second;
}

void CheckGuest(std::uint64_t address) {
    const auto image = Find(address);
    Require(image.frameHeader != 0);
    ModuleInfoForUnwind info{};
    Require(sceKernelGetModuleInfoForUnwind(address, 1, &info) == 0);
    Require(info.st_size == sizeof(ModuleInfoForUnwind));
    Require(info.eh_frame_hdr_addr == image.frameHeader);
    const auto* header = reinterpret_cast<const std::uint8_t*>(image.frameHeader);
    Require(header[0] == 1 && header[1] == 0x1b);
    std::int32_t relative = 0;
    std::memcpy(&relative, header + 4, 4);
    const auto frames = image.frameHeader + 4 + static_cast<std::int64_t>(relative);
    Require(info.eh_frame_addr == frames);
    auto record = reinterpret_cast<const std::uint8_t*>(frames);
    for (;;) {
        std::uint32_t length = 0;
        std::memcpy(&length, record, 4);
        if (length == 0) break;
        record += 4 + length;
    }
    Require(info.eh_frame_size == static_cast<std::uint64_t>(record + 4 - reinterpret_cast<const std::uint8_t*>(frames)));
    Require(info.seg0_addr == image.firstLoad && info.seg0_size == image.firstSize);
}

}

int main(int argc, char** argv) {
    Require(argc == 2);
    CheckGuest(reinterpret_cast<std::uint64_t>(&main));

    const auto guest = std::filesystem::absolute("anyps5-unwind-info-module-for-test.guest.prx");
    std::filesystem::copy_file(argv[1], guest, std::filesystem::copy_options::overwrite_existing);
    void* module = ::dlopen(guest.c_str(), RTLD_NOW | RTLD_LOCAL);
    Require(module != nullptr);
    CheckGuest(FirstLoadOf(".guest.prx"));
    Require(::dlclose(module) == 0);
    std::filesystem::remove(guest);

    const auto host = reinterpret_cast<std::uint64_t>(&std::abort);
    const auto hostImage = Find(host);
    Require(!hostImage.name.empty() && hostImage.frameHeader != 0);
    ModuleInfoForUnwind info{};
    Require(sceKernelGetModuleInfoForUnwind(host, 1, &info) == 0);
    Require(info.eh_frame_hdr_addr == 0 && info.eh_frame_addr == 0 && info.eh_frame_size == 0);
    Require(info.seg0_addr == hostImage.firstLoad && info.seg0_size == hostImage.firstSize);

    Require(sceKernelGetModuleInfoForUnwind(0x10000, 1, &info) == SCE_KERNEL_ERROR_ESRCH);
}
