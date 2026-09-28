#include "Optimization/RequestMemoryView.hpp"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace ShaderRecompiler {

namespace {

[[noreturn]] void fail(const char* message) {
    throw std::runtime_error(message);
}

}

RequestMemoryView::RequestMemoryView(std::span<const MemoryRegion> memoryRegions) {
    regions.assign(memoryRegions.begin(), memoryRegions.end());
    for (const MemoryRegion& region : regions) {
        if (region.bytes.empty()) {
            fail("RequestMemoryView::RequestMemoryView memory region is empty");
        }
        if (region.guestAddress > std::numeric_limits<std::uint64_t>::max() - region.bytes.size()) {
            fail("RequestMemoryView::RequestMemoryView memory region address range overflows");
        }
    }
    std::sort(regions.begin(), regions.end(), [](const MemoryRegion& left, const MemoryRegion& right) {
        return left.guestAddress < right.guestAddress;
    });
    for (std::size_t index = 1; index < regions.size(); index++) {
        const std::uint64_t previousEnd = regions[index - 1].guestAddress + regions[index - 1].bytes.size();
        if (regions[index].guestAddress < previousEnd) {
            fail("RequestMemoryView::RequestMemoryView memory regions overlap");
        }
    }
}

bool RequestMemoryView::ReadGuestMemory(void* userContext, std::uint64_t address, std::uint32_t* value) {
    const auto* self = static_cast<const RequestMemoryView*>(userContext);
    static const bool debug = std::getenv("APS5_SRT_DEBUG") != nullptr;
    const auto miss = [&] {
        if (debug) std::fprintf(stderr, "[srt] guest read 0x%llx is outside the request memory\n", static_cast<unsigned long long>(address));
        return false;
    };
    const auto it = std::upper_bound(self->regions.begin(), self->regions.end(), address, [](std::uint64_t addressValue, const MemoryRegion& region) {
        return addressValue < region.guestAddress;
    });
    if (it == self->regions.begin()) {
        return miss();
    }
    const MemoryRegion& region = *std::prev(it);
    if (address < region.guestAddress) {
        return miss();
    }
    const std::uint64_t offset = address - region.guestAddress;
    if (offset > region.bytes.size() || region.bytes.size() - offset < sizeof(std::uint32_t)) {
        return miss();
    }
    std::uint32_t word = 0;
    std::memcpy(&word, region.bytes.data() + offset, sizeof(word));
    *value = word;
    return true;
}

SrtRuntime RequestMemoryView::MakeRuntime(std::span<const std::uint32_t> userData, std::uint64_t shaderBase) {
    SrtRuntime runtime;
    runtime.userData = userData;
    runtime.shaderBase = shaderBase;
    runtime.userContext = this;
    runtime.readMemory = &ReadGuestMemory;
    runtime.readSpecializationMemory = &ReadGuestMemory;
    return runtime;
}

}
