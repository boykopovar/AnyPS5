#include "GuestKernelModel.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace {
constexpr std::int64_t BadFd = -9;
constexpr std::int64_t Fault = -14;
constexpr std::int64_t Invalid = -22;
constexpr std::int64_t Again = -35;
constexpr std::int64_t NotImplemented = -78;
}

void GuestKernelModel::Map(std::vector<Region>& regions, std::uint64_t address, std::size_t size) {
    if (!size || size > std::numeric_limits<std::uint64_t>::max() - address)
        throw std::invalid_argument("invalid guest memory range");
    for (const auto& region : regions) {
        const auto end = region.address + region.bytes.size();
        if (address < end && region.address < address + size)
            throw std::invalid_argument("overlapping guest memory ranges");
    }
    regions.push_back({address, std::vector<std::byte>(size)});
}

std::span<std::byte> GuestKernelModel::Locate(std::vector<Region>& regions, std::uint64_t address, std::size_t size) {
    for (auto& region : regions) {
        if (address >= region.address && address - region.address <= region.bytes.size() &&
            size <= region.bytes.size() - (address - region.address))
            return {region.bytes.data() + (address - region.address), size};
    }
    return {};
}

std::span<const std::byte> GuestKernelModel::Locate(const std::vector<Region>& regions, std::uint64_t address, std::size_t size) {
    for (const auto& region : regions) {
        if (address >= region.address && address - region.address <= region.bytes.size() &&
            size <= region.bytes.size() - (address - region.address))
            return {region.bytes.data() + (address - region.address), size};
    }
    return {};
}

void GuestKernelModel::MapUser(std::uint64_t address, std::size_t size) { Map(user_, address, size); }
void GuestKernelModel::MapKernel(std::uint64_t address, std::size_t size) { Map(kernel_, address, size); }

bool GuestKernelModel::ReadUser(std::uint64_t address, std::span<std::byte> output) const {
    const auto source = Locate(user_, address, output.size());
    if (source.size() != output.size()) return false;
    std::copy(source.begin(), source.end(), output.begin());
    return true;
}
bool GuestKernelModel::WriteUser(std::uint64_t address, std::span<const std::byte> input) {
    const auto target = Locate(user_, address, input.size());
    if (target.size() != input.size()) return false;
    std::copy(input.begin(), input.end(), target.begin());
    return true;
}
bool GuestKernelModel::CopyKernelOut(std::uint64_t address, std::span<std::byte> output) const {
    const auto source = Locate(kernel_, address, output.size());
    if (source.size() != output.size()) return false;
    std::copy(source.begin(), source.end(), output.begin());
    return true;
}
bool GuestKernelModel::CopyKernelIn(std::uint64_t address, std::span<const std::byte> input) {
    const auto target = Locate(kernel_, address, input.size());
    if (target.size() != input.size()) return false;
    std::copy(input.begin(), input.end(), target.begin());
    return true;
}

std::array<int, 2> GuestKernelModel::CreatePipe() {
    if (nextFd_ > std::numeric_limits<int>::max() - 2)
        throw std::overflow_error("guest pipe descriptor space exhausted");
    const std::array<int, 2> pair{nextFd_, nextFd_ + 1};
    nextFd_ += 2;
    pipes_.push_back({pair[0], pair[1], true, true, {}});
    return pair;
}

std::int64_t GuestKernelModel::Dispatch(std::uint64_t number, const std::array<std::uint64_t, 6>& args) {
    if (number == 20) return pid_;
    if (number == 542) {
        if (args[1] != 0) return Invalid; // Only ordinary pipes are modeled.
        if (Locate(user_, args[0], 2 * sizeof(std::int32_t)).size() != 2 * sizeof(std::int32_t)) return Fault;
        const auto pair = CreatePipe();
        std::array<std::byte, 8> bytes{};
        for (int index = 0; index < 2; ++index)
            for (int shift = 0; shift < 4; ++shift)
                bytes[index * 4 + shift] = std::byte((pair[index] >> (shift * 8)) & 0xff);
        WriteUser(args[0], bytes);
        return 0;
    }
    if (number != 3 && number != 4 && number != 6) return NotImplemented;
    const auto fd = static_cast<int>(args[0]);
    for (auto& pipe : pipes_) {
        if (number == 6) {
            if (fd == pipe.readFd && pipe.readOpen) { pipe.readOpen = false; return 0; }
            if (fd == pipe.writeFd && pipe.writeOpen) { pipe.writeOpen = false; return 0; }
            continue;
        }
        if (args[2] > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) return Invalid;
        const auto size = static_cast<std::size_t>(args[2]);
        if (number == 4 && fd == pipe.writeFd && pipe.writeOpen && pipe.readOpen) {
            const auto source = Locate(user_, args[1], size);
            if (source.size() != size) return Fault;
            pipe.bytes.insert(pipe.bytes.end(), source.begin(), source.end());
            return static_cast<std::int64_t>(size);
        }
        if (number == 3 && fd == pipe.readFd && pipe.readOpen) {
            const auto count = std::min(size, pipe.bytes.size());
            const auto target = Locate(user_, args[1], size);
            if (target.size() != size) return Fault;
            if (!count) return pipe.writeOpen ? Again : 0;
            for (std::size_t index = 0; index < count; ++index) {
                target[index] = pipe.bytes.front();
                pipe.bytes.pop_front();
            }
            return static_cast<std::int64_t>(count);
        }
    }
    return BadFd;
}
