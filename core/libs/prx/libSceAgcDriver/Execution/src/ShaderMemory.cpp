#include "prx/libSceAgcDriver/Execution/include/ShaderMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "Optimization/RequestMemoryView.hpp"
#include "Optimization/ResourceMaterializer.hpp"
#include "Optimization/ResourceProgram.hpp"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace AgcDriver {
namespace {

struct CaptureProfile {
    std::atomic<std::uint64_t> captures{0};
    std::atomic<std::uint64_t> reads{0};
    std::atomic<std::uint64_t> pages{0};
    // The whole CaptureResources call: the plan lookup and the materialization are one call.
    std::atomic<std::uint64_t> captureNanoseconds{0};
};

CaptureProfile& CaptureTotals() {
    static CaptureProfile profile;
    return profile;
}

}

ShaderMemory::ShaderMemory(std::span<const ShaderRecompiler::MemoryRegion> regions) {
    const ShaderRecompiler::RequestMemoryView validated(regions);
    for (const auto& region : regions) {
        initial.emplace(region.guestAddress, region.bytes);
    }
}

ShaderMemory::Page& ShaderMemory::page(std::uint64_t base) {
    const auto found = pages.find(base);
    if (found != pages.end()) return found->second;
    auto& page = pages[base];
    ++CaptureTotals().pages;
    // A page is either mapped whole or read word by word where the guest mapped less than a page.
    if (GuestMemory::Accessible(reinterpret_cast<const void*>(base), PageBytes)) {
        GuestMemory::Read(base, std::as_writable_bytes(std::span(page.words)), sizeof(std::uint32_t));
        page.valid.set();
    }
    return page;
}

bool ShaderMemory::read(void* context, std::uint64_t address, std::uint32_t* value) {
    auto& self = *static_cast<ShaderMemory*>(context);
    if (address % sizeof(*value) != 0 || address > std::numeric_limits<std::uint64_t>::max() - sizeof(*value)) {
        throw std::runtime_error("AGC driver: invalid shader memory read address");
    }
    ++CaptureTotals().reads;
    if (!self.initial.empty()) {
        const auto next = self.initial.upper_bound(address);
        if (next != self.initial.begin()) {
            const auto previous = std::prev(next);
            const auto offset = address - previous->first;
            if (offset < previous->second.size()) {
                if (previous->second.size() - offset < sizeof(*value)) throw std::runtime_error("AGC driver: shader memory read crosses a snapshot boundary");
                std::memcpy(value, previous->second.data() + offset, sizeof(*value));
                return true;
            }
        }
        if (next != self.initial.end() && next->first - address < sizeof(*value)) throw std::runtime_error("AGC driver: shader memory read overlaps a snapshot boundary");
    }
    auto& page = self.page(address & ~static_cast<std::uint64_t>(PageBytes - 1));
    const auto index = static_cast<std::size_t>((address % PageBytes) / sizeof(*value));
    if (!page.valid.test(index)) {
        std::uint32_t word = 0;
        GuestMemory::Read(address, std::as_writable_bytes(std::span(&word, 1)), alignof(std::uint32_t));
        page.words[index] = word;
        page.valid.set(index);
    }
    page.read.set(index);
    *value = page.words[index];
    return true;
}

std::shared_ptr<const ShaderRecompiler::ResourceCapture> ShaderMemory::Capture(const ShaderRecompiler::RecompileRequest& request) {
    // The capture's word and page reads (through `read`) are attributed to it ([hooksync], [guestmem]).
    const GuestMemory::ReadSiteScope site(GuestMemory::ReadSite::Capture);
    const auto started = std::chrono::steady_clock::now();
    ShaderRecompiler::SrtRuntime runtime;
    runtime.userData = request.context.userData;
    runtime.shaderBase = request.shader.codeAddress;
    runtime.userContext = this;
    runtime.readMemory = &read;
    runtime.readSpecializationMemory = &read;
    auto capture = ShaderRecompiler::CaptureResources(request, runtime);
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    auto& totals = CaptureTotals();
    if (profile) {
        const auto done = std::chrono::steady_clock::now();
        totals.captureNanoseconds += static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(done - started).count());
        std::uint64_t initialBytes = 0;
        for (const auto& [address, bytes] : initial) initialBytes += bytes.size();
        if (++totals.captures % 500 == 0) std::fprintf(stderr, "[capture] %llu captures: %llu word reads, %llu pages fetched, capture (plan lookup + materialize) %.1f s, %llu KiB registered code this capture\n", static_cast<unsigned long long>(totals.captures.load()), static_cast<unsigned long long>(totals.reads.load()), static_cast<unsigned long long>(totals.pages.load()), totals.captureNanoseconds.load() / 1e9, static_cast<unsigned long long>(initialBytes / 1024));
    }
    return capture;
}

std::vector<ShaderRecompiler::MemoryRegion> ShaderMemory::Regions() const {
    std::vector<ShaderRecompiler::MemoryRegion> result;
    result.reserve(initial.size() + pages.size());
    auto next = initial.begin();
    // Both maps are ordered by address and never overlap, so a merge keeps the result sorted.
    for (const auto& [base, page] : pages) {
        while (next != initial.end() && next->first < base) {
            result.push_back({next->first, next->second});
            ++next;
        }
        for (std::size_t index = 0; index < PageWords;) {
            if (!page.read.test(index)) {
                ++index;
                continue;
            }
            const auto first = index;
            while (index < PageWords && page.read.test(index)) ++index;
            result.push_back({base + first * sizeof(std::uint32_t), std::as_bytes(std::span(page.words).subspan(first, index - first))});
        }
    }
    for (; next != initial.end(); ++next) result.push_back({next->first, next->second});
    return result;
}

}
