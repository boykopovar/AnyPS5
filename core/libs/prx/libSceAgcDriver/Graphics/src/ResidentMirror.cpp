#include "prx/libSceAgcDriver/Graphics/include/ResidentMirror.hpp"
#include "prx/libSceAgcDriver/Graphics/include/BufferPool.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libc/include/GuestArena.hpp"
#ifndef _WIN32
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#endif
#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>

namespace AgcDriver::Graphics {

namespace {

constexpr std::uint64_t BlockSize = ResidentBuffer::BlockBytes;
constexpr std::uint64_t SlabBytes = std::uint64_t{16} << 20u;
constexpr std::uint32_t SlabBlocks = static_cast<std::uint32_t>(SlabBytes / BlockSize);
constexpr std::uint64_t ZeroBytes = SlabBytes;
constexpr VkAccessFlags TransferredAccess = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_UNIFORM_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_INDIRECT_COMMAND_READ_BIT;

[[noreturn]] void fail(const char* step, VkResult result) {
    char text[160];
    std::snprintf(text, sizeof(text), "%s failed (%d)", step, static_cast<int>(result));
    throw std::runtime_error(text);
}

struct TransientBuffer {
    TransientBuffer(const Context& context, VkBuffer buffer) : context(context), buffer(buffer) {}
    ~TransientBuffer() { context.Function<PFN_vkDestroyBuffer>("vkDestroyBuffer")(context.device, buffer, nullptr); }
    TransientBuffer(const TransientBuffer&) = delete;
    TransientBuffer& operator=(const TransientBuffer&) = delete;
    Context context;
    VkBuffer buffer;
};

template<typename Work>
void recordTransfer(const Context& context, Work&& work) {
    GuestMemory::AssertGpuLockHeld("heap mirror transfer");
    std::vector<std::shared_ptr<void>> kept;
    if (auto* recorder = Recorder::Active(); recorder != nullptr) {
        const auto commands = recorder->Commands();
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT);
        work(commands, kept);
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, TransferredAccess);
        for (auto& object : kept) recorder->Keep(std::move(object));
        recorder->MarkCovered(TransferredAccess);
        return;
    }
    CommandBatch batch(context);
    RecordMemoryBarrier(context, batch.Handle(), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT);
    work(batch.Handle(), kept);
    RecordMemoryBarrier(context, batch.Handle(), VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, TransferredAccess);
    batch.SubmitAndWait();
}

}

class ResidentMemory {
public:
    struct Slab {
        VkDeviceMemory memory = VK_NULL_HANDLE;
        std::unique_ptr<std::byte[]> host;
        std::vector<std::uint32_t> free;
    };

    ResidentMemory(const Context& context, std::uint32_t type) : context(context), type(type) {
        zero = allocate(ZeroBytes, "vkAllocateMemory of the heap mirrors' zero block");
    }

    ~ResidentMemory() {
        const auto freeMemory = context.Function<PFN_vkFreeMemory>("vkFreeMemory");
        for (auto& slab : slabs) freeMemory(context.device, slab->memory, nullptr);
        freeMemory(context.device, zero, nullptr);
    }

    ResidentMemory(const ResidentMemory&) = delete;
    ResidentMemory& operator=(const ResidentMemory&) = delete;

    VkDeviceMemory Zero() const { return zero; }
    bool Zeroed() const { return zeroed.load(std::memory_order_acquire); }

    void FillZero(VkCommandBuffer commands, std::vector<std::shared_ptr<void>>& kept) {
        if (Zeroed()) return;
        VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        info.size = ZeroBytes;
        info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        VkBuffer buffer = VK_NULL_HANDLE;
        if (const auto result = context.Function<PFN_vkCreateBuffer>("vkCreateBuffer")(context.device, &info, nullptr, &buffer); result != VK_SUCCESS) fail("vkCreateBuffer of the heap mirrors' zero fill", result);
        auto transient = std::make_shared<TransientBuffer>(context, buffer);
        VkMemoryRequirements requirements{};
        context.Function<PFN_vkGetBufferMemoryRequirements>("vkGetBufferMemoryRequirements")(context.device, buffer, &requirements);
        if ((requirements.memoryTypeBits & (1u << type)) == 0 || requirements.size > ZeroBytes) throw std::runtime_error("the heap mirrors' zero block memory cannot back a transfer buffer to fill it");
        if (const auto result = context.Function<PFN_vkBindBufferMemory>("vkBindBufferMemory")(context.device, buffer, zero, 0); result != VK_SUCCESS) fail("vkBindBufferMemory of the heap mirrors' zero fill", result);
        context.Function<PFN_vkCmdFillBuffer>("vkCmdFillBuffer")(commands, buffer, 0, VK_WHOLE_SIZE, 0);
        kept.push_back(std::move(transient));
        zeroed.store(true, std::memory_order_release);
    }

    std::tuple<Slab*, std::uint32_t> Take() {
        std::lock_guard lock(mutex);
        for (auto& slab : slabs) {
            if (slab->free.empty()) continue;
            const auto index = slab->free.back();
            slab->free.pop_back();
            return {slab.get(), index};
        }
        auto slab = std::make_unique<Slab>();
        slab->memory = allocate(SlabBytes, "vkAllocateMemory of a heap mirror slab");
        slab->host = std::unique_ptr<std::byte[]>(new std::byte[SlabBytes]);
        for (std::uint32_t index = SlabBlocks; index-- > 1;) slab->free.push_back(index);
        slabs.push_back(std::move(slab));
        return {slabs.back().get(), 0u};
    }

    void Put(Slab* slab, std::uint32_t index) noexcept {
        std::lock_guard lock(mutex);
        slab->free.push_back(index);
        if (slab->free.size() != SlabBlocks) return;
        const auto found = std::find_if(slabs.begin(), slabs.end(), [&](const auto& held) { return held.get() == slab; });
        context.Function<PFN_vkFreeMemory>("vkFreeMemory")(context.device, slab->memory, nullptr);
        slabs.erase(found);
    }

private:
    VkDeviceMemory allocate(std::uint64_t bytes, const char* operation) {
        const VkMemoryAllocateFlagsInfo flags{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO, nullptr, VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT, 0};
        VkMemoryAllocateInfo info{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, context.bufferDeviceAddress ? &flags : nullptr};
        info.allocationSize = bytes;
        info.memoryTypeIndex = type;
        if (context.bufferPool != nullptr) return context.bufferPool->AllocateMemory(info, operation);
        VkDeviceMemory memory = VK_NULL_HANDLE;
        if (const auto result = context.Function<PFN_vkAllocateMemory>("vkAllocateMemory")(context.device, &info, nullptr, &memory); result != VK_SUCCESS) fail(operation, result);
        return memory;
    }

    Context context;
    std::uint32_t type;
    VkDeviceMemory zero = VK_NULL_HANDLE;
    std::atomic<bool> zeroed{false};
    std::mutex mutex;
    std::vector<std::unique_ptr<Slab>> slabs;
};

namespace {

struct ResidentDevices {
    std::mutex mutex;
    std::map<std::tuple<VkDevice, std::uint32_t>, std::weak_ptr<ResidentMemory>> memories;
    std::map<VkDevice, std::uint64_t> addressSpace;
    std::uint64_t limit = 0;
    std::mutex pendingMutex;
    std::vector<std::weak_ptr<ResidentBuffer>> pending;
};

ResidentDevices& Devices() {
    static ResidentDevices devices;
    return devices;
}

std::shared_ptr<ResidentMemory> residentMemory(const Context& context, std::uint32_t type) {
    auto& devices = Devices();
    std::lock_guard lock(devices.mutex);
    auto& slot = devices.memories[{context.device, type}];
    if (auto held = slot.lock()) return held;
    auto made = std::make_shared<ResidentMemory>(context, type);
    slot = made;
    return made;
}

struct Candidate {
    std::uint64_t begin = 0;
    std::uint64_t end = 0;
    bool readable = true;
};

bool allZero(std::uint64_t begin, std::uint64_t end) {
    const auto* at = reinterpret_cast<const unsigned char*>(begin);
    const auto* stop = reinterpret_cast<const unsigned char*>(end);
    while (at < stop && reinterpret_cast<std::uintptr_t>(at) % sizeof(std::uint64_t) != 0) {
        if (*at++ != 0) return false;
    }
    while (stop - at >= static_cast<std::ptrdiff_t>(sizeof(std::uint64_t))) {
        std::uint64_t word = 0;
        std::memcpy(&word, at, sizeof(word));
        if (word != 0) return false;
        at += sizeof(word);
    }
    while (at < stop) {
        if (*at++ != 0) return false;
    }
    return true;
}

#ifndef _WIN32
struct Mapping {
    std::uint64_t begin = 0;
    std::uint64_t end = 0;
    bool readable = false;
    bool privateAnonymous = false;
};

std::vector<Mapping> mappings(std::uint64_t begin, std::uint64_t end) {
    std::ifstream maps("/proc/self/maps");
    Require(static_cast<bool>(maps), "cannot read /proc/self/maps for a heap mirror's pages");
    std::vector<Mapping> found;
    std::string line;
    while (std::getline(maps, line)) {
        std::istringstream fields(line);
        std::string span, permissions, offset, device;
        unsigned long long inode = 0;
        fields >> span >> permissions >> offset >> device >> inode;
        const auto dash = span.find('-');
        Require(dash != std::string::npos && permissions.size() == 4, "unexpected /proc/self/maps line: " + line);
        const auto first = std::stoull(span.substr(0, dash), nullptr, 16);
        const auto last = std::stoull(span.substr(dash + 1), nullptr, 16);
        if (last <= begin || first >= end) continue;
        found.push_back({std::max<std::uint64_t>(first, begin), std::min<std::uint64_t>(last, end), permissions[0] == 'r', inode == 0 && permissions[3] == 'p'});
    }
    return found;
}

void fileData(std::vector<Candidate>& maybe, const Mapping& mapping, int file, std::uint64_t offset) {
    const auto end = offset + (mapping.end - mapping.begin);
    auto at = static_cast<off_t>(offset);
    while (static_cast<std::uint64_t>(at) < end) {
        const auto data = lseek(file, at, SEEK_DATA);
        if (data < 0) {
            Require(errno == ENXIO, "SEEK_DATA on a heap mirror's shared backing failed");
            return;
        }
        if (static_cast<std::uint64_t>(data) >= end) return;
        const auto hole = lseek(file, data, SEEK_HOLE);
        Require(hole >= 0, "SEEK_HOLE on a heap mirror's shared backing failed");
        const auto stop = std::min<std::uint64_t>(static_cast<std::uint64_t>(hole), end);
        maybe.push_back({mapping.begin + (static_cast<std::uint64_t>(data) - offset), mapping.begin + (stop - offset), mapping.readable});
        at = static_cast<off_t>(stop);
    }
}

void populatedPages(std::vector<Candidate>& maybe, const Mapping& mapping) {
    static const auto page = static_cast<std::uint64_t>(sysconf(_SC_PAGESIZE));
    const int file = open("/proc/self/pagemap", O_RDONLY | O_CLOEXEC);
    Require(file >= 0, "cannot open /proc/self/pagemap for a heap mirror's pages");
    constexpr std::size_t Entries = 4096;
    std::array<std::uint64_t, Entries> entries{};
    const auto first = mapping.begin / page;
    const auto last = (mapping.end + page - 1) / page;
    for (auto at = first; at < last;) {
        const auto count = static_cast<std::size_t>(std::min<std::uint64_t>(Entries, last - at));
        const auto read = pread(file, entries.data(), count * sizeof(std::uint64_t), static_cast<off_t>(at * sizeof(std::uint64_t)));
        if (read != static_cast<ssize_t>(count * sizeof(std::uint64_t))) {
            close(file);
            throw std::runtime_error("reading /proc/self/pagemap for a heap mirror's pages failed");
        }
        for (std::size_t index = 0; index < count; ++index) {
            constexpr std::uint64_t present = std::uint64_t{1} << 63u;
            constexpr std::uint64_t swapped = std::uint64_t{1} << 62u;
            if ((entries[index] & (present | swapped)) == 0) continue;
            const auto address = (at + index) * page;
            const auto from = std::max(address, mapping.begin);
            const auto to = std::min(address + page, mapping.end);
            if (!maybe.empty() && maybe.back().end == from && maybe.back().readable == mapping.readable) maybe.back().end = to;
            else maybe.push_back({from, to, mapping.readable});
        }
        at += count;
    }
    close(file);
}
#endif

std::vector<Candidate> possiblyNonzero(std::uint64_t begin, std::uint64_t end) {
    std::vector<Candidate> maybe;
#ifdef _WIN32
    maybe.push_back({begin, end, true});
#else
    for (const auto& mapping : mappings(begin, end)) {
        int file = -1;
        std::uint64_t offset = 0;
        if (GuestArena::GuestArenaSharedBacking_nid_postfix(static_cast<std::uintptr_t>(mapping.begin), static_cast<std::size_t>(mapping.end - mapping.begin), &file, &offset)) {
            try {
                fileData(maybe, mapping, file, offset);
            } catch (...) {
                close(file);
                throw;
            }
            close(file);
        } else if (mapping.privateAnonymous) {
            populatedPages(maybe, mapping);
        } else {
            maybe.push_back({mapping.begin, mapping.end, mapping.readable});
        }
    }
#endif
    return maybe;
}

}

bool GuestBytesZero(std::uint64_t begin, std::uint64_t end) {
    return allZero(begin, end);
}

GuestRuns GuestNonzeroBlocks(const GuestRuns& candidates) {
    GuestRuns runs;
    for (const auto& [first, last] : candidates) {
        if (first >= last) continue;
        const auto maybe = possiblyNonzero(first, last);
        std::size_t at = 0;
        for (auto block = first / BlockSize * BlockSize; block < last; block += BlockSize) {
            const auto from = std::max(first, block);
            const auto to = std::min(last, block + BlockSize);
            while (at < maybe.size() && maybe[at].end <= from) ++at;
            bool nonzero = false;
            for (auto next = at; !nonzero && next < maybe.size() && maybe[next].begin < to; ++next) {
                const auto low = std::max(from, maybe[next].begin);
                const auto high = std::min(to, maybe[next].end);
                if (low < high) nonzero = !maybe[next].readable || !allZero(low, high);
            }
            if (!nonzero) continue;
            if (!runs.empty() && runs.back().second == from) runs.back().second = to;
            else runs.emplace_back(from, to);
        }
    }
    return runs;
}

void SetResidentAddressSpaceLimit(std::uint64_t bytes) {
    auto& devices = Devices();
    std::lock_guard lock(devices.mutex);
    devices.limit = bytes;
}

std::shared_ptr<ResidentBuffer> ResidentBuffer::Create(const Context& context, std::uint64_t base, std::uint64_t bytes, const GuestRuns& backed) {
    if (!context.sparseBinding || !context.sparseResidencyBuffer || !context.sparseResidencyAliased) return nullptr;
    const auto first = base / BlockSize * BlockSize;
    const auto size = (base - first + bytes + BlockSize - 1) / BlockSize * BlockSize;
    {
        auto& devices = Devices();
        std::lock_guard lock(devices.mutex);
        const auto limit = devices.limit != 0 ? std::min(devices.limit, context.limits.sparseAddressSpaceSize) : context.limits.sparseAddressSpaceSize;
        auto& used = devices.addressSpace[context.device];
        if (size > limit || used > limit - size) return nullptr;
        used += size;
    }
    return std::shared_ptr<ResidentBuffer>(new ResidentBuffer(context, base, bytes, backed));
}

ResidentBuffer::ResidentBuffer(const Context& context, std::uint64_t base, std::uint64_t bytes, const GuestRuns& backed) : context(context) {
    origin = base / BlockSize * BlockSize;
    lead = base - origin;
    size = (lead + bytes + BlockSize - 1) / BlockSize * BlockSize;
    try {
        VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        info.flags = VK_BUFFER_CREATE_SPARSE_BINDING_BIT | VK_BUFFER_CREATE_SPARSE_ALIASED_BIT;
        info.size = size;
        info.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        const std::array<std::uint32_t, 2> families{context.queueFamily, context.sparseFamily};
        if (context.sparseQueue != VK_NULL_HANDLE && context.sparseFamily != context.queueFamily) {
            info.sharingMode = VK_SHARING_MODE_CONCURRENT;
            info.queueFamilyIndexCount = static_cast<std::uint32_t>(families.size());
            info.pQueueFamilyIndices = families.data();
        }
        if (const auto result = context.Function<PFN_vkCreateBuffer>("vkCreateBuffer")(context.device, &info, nullptr, &buffer); result != VK_SUCCESS) fail("vkCreateBuffer of a resident heap mirror", result);
        VkMemoryRequirements requirements{};
        context.Function<PFN_vkGetBufferMemoryRequirements>("vkGetBufferMemoryRequirements")(context.device, buffer, &requirements);
        if (requirements.alignment == 0 || requirements.alignment > BlockSize || BlockSize % requirements.alignment != 0) {
            char text[160];
            std::snprintf(text, sizeof(text), "the device's sparse buffer block of %llu bytes does not divide a heap mirror's 64 KiB block", static_cast<unsigned long long>(requirements.alignment));
            throw std::runtime_error(text);
        }
        type = context.DeviceMemoryType(requirements.memoryTypeBits, SlabBytes, "a resident heap mirror slab");
        memory = residentMemory(context, type);
        slots.resize(static_cast<std::size_t>(size / BlockSize));
        std::vector<bool> wanted(slots.size(), false);
        for (const auto& [first, last] : backed) {
            if (first >= last) continue;
            for (auto index = blockIndex(first); index <= blockIndex(last - 1); ++index) wanted[index] = true;
        }
        std::vector<VkSparseMemoryBind> binds;
        for (std::size_t index = 0; index < slots.size();) {
            if (wanted[index]) {
                auto [slab, at] = memory->Take();
                slots[index] = {slab, at, slab->host.get() + at * BlockSize};
                std::memset(slots[index].host, 0, static_cast<std::size_t>(BlockSize));
                binds.push_back({index * BlockSize, BlockSize, slab->memory, at * BlockSize, 0});
                ++bound;
                ++index;
                continue;
            }
            auto last = index;
            while (last < slots.size() && !wanted[last] && (last - index + 1) * BlockSize <= ZeroBytes) ++last;
            binds.push_back({index * BlockSize, (last - index) * BlockSize, memory->Zero(), 0, 0});
            index = last;
        }
        submit(binds);
        const VkBufferDeviceAddressInfo addressInfo{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, nullptr, buffer};
        address = context.Function<PFN_vkGetBufferDeviceAddressKHR>("vkGetBufferDeviceAddressKHR")(context.device, &addressInfo);
        if (address == 0) fail("vkGetBufferDeviceAddressKHR of a resident heap mirror", VK_ERROR_UNKNOWN);
    } catch (...) {
        release();
        throw;
    }
}

ResidentBuffer::~ResidentBuffer() {
    release();
}

void ResidentBuffer::release() noexcept {
    if (buffer != VK_NULL_HANDLE) context.Function<PFN_vkDestroyBuffer>("vkDestroyBuffer")(context.device, buffer, nullptr);
    buffer = VK_NULL_HANDLE;
    for (auto& slot : slots) {
        if (slot.host != nullptr) memory->Put(static_cast<ResidentMemory::Slab*>(slot.slab), slot.index);
        slot = {};
    }
    auto& devices = Devices();
    std::lock_guard lock(devices.mutex);
    if (const auto found = devices.addressSpace.find(context.device); found != devices.addressSpace.end()) found->second -= std::min(found->second, size);
    size = 0;
}

std::size_t ResidentBuffer::blockIndex(std::uint64_t guest) const {
    Require(guest >= origin && guest - origin < size, "guest address outside its resident heap mirror");
    return static_cast<std::size_t>((guest - origin) / BlockSize);
}

std::byte* ResidentBuffer::Block(std::uint64_t guest) const {
    const auto& slot = slots[blockIndex(guest)];
    return slot.host == nullptr ? nullptr : slot.host + (guest - origin) % BlockSize;
}

std::uint64_t ResidentBuffer::UnboundBytes(const GuestRuns& runs) const {
    std::vector<std::size_t> seen;
    for (const auto& [first, last] : runs) {
        if (first >= last) continue;
        for (auto index = blockIndex(first); index <= blockIndex(last - 1); ++index) {
            if (slots[index].host == nullptr) seen.push_back(index);
        }
    }
    std::sort(seen.begin(), seen.end());
    return static_cast<std::uint64_t>(std::unique(seen.begin(), seen.end()) - seen.begin()) * BlockSize;
}

void ResidentBuffer::Bind(const GuestRuns& runs) {
    std::vector<VkSparseMemoryBind> binds;
    std::vector<std::size_t> made;
    for (const auto& [first, last] : runs) {
        if (first >= last) continue;
        for (auto index = blockIndex(first); index <= blockIndex(last - 1); ++index) {
            if (slots[index].host != nullptr) continue;
            auto [slab, at] = memory->Take();
            slots[index] = {slab, at, slab->host.get() + at * BlockSize};
            std::memset(slots[index].host, 0, static_cast<std::size_t>(BlockSize));
            binds.push_back({index * BlockSize, BlockSize, slab->memory, at * BlockSize, 0});
            made.push_back(index);
        }
    }
    if (binds.empty()) return;
    if (auto* recorder = Recorder::Active(); recorder != nullptr) recorder->Sync();
    try {
        submit(binds);
    } catch (...) {
        for (const auto index : made) {
            memory->Put(static_cast<ResidentMemory::Slab*>(slots[index].slab), slots[index].index);
            slots[index] = {};
        }
        throw;
    }
    bound += made.size();
    for (const auto index : made) MarkChanged(origin + index * BlockSize, origin + (index + 1) * BlockSize);
}

void ResidentBuffer::MarkChanged(std::uint64_t begin, std::uint64_t end) {
    if (begin >= end) return;
    bool first = false;
    {
        std::lock_guard lock(changedMutex);
        first = changed.empty();
        if (!changed.empty() && changed.back().second == begin) changed.back().second = end;
        else changed.emplace_back(begin, end);
    }
    if (!first) return;
    auto& devices = Devices();
    std::lock_guard lock(devices.pendingMutex);
    devices.pending.push_back(weak_from_this());
}

void ResidentBuffer::Read(std::uint64_t guest, std::span<std::byte> out) const {
    std::size_t done = 0;
    while (done < out.size()) {
        const auto at = guest + done;
        const auto within = static_cast<std::size_t>((at - origin) % BlockSize);
        const auto length = std::min<std::size_t>(out.size() - done, static_cast<std::size_t>(BlockSize) - within);
        if (const auto* source = Block(at); source != nullptr) std::memcpy(out.data() + done, source, length);
        else std::memset(out.data() + done, 0, length);
        done += length;
    }
}

void ResidentBuffer::Store(std::uint64_t guest, std::span<const std::byte> bytes) {
    std::size_t done = 0;
    while (done < bytes.size()) {
        const auto at = guest + done;
        const auto within = static_cast<std::size_t>((at - origin) % BlockSize);
        const auto length = std::min<std::size_t>(bytes.size() - done, static_cast<std::size_t>(BlockSize) - within);
        if (auto* target = Block(at); target != nullptr) std::memcpy(target, bytes.data() + done, length);
        else Require(GuestBytesZero(reinterpret_cast<std::uintptr_t>(bytes.data() + done), reinterpret_cast<std::uintptr_t>(bytes.data() + done + length)), "the GPU wrote a heap mirror block that holds no memory");
        done += length;
    }
}

std::shared_ptr<Buffer> ResidentBuffer::RecordReadback(VkCommandBuffer commands, std::uint64_t guest, std::uint64_t bytes) const {
    Require(guest >= origin && bytes <= size && guest - origin <= size - bytes, "heap mirror readback outside its buffer");
    auto staging = std::make_shared<Buffer>(context, static_cast<std::size_t>(bytes), VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    CopyBuffer(context, commands, buffer, guest - origin, staging->Handle(), 0, bytes);
    return staging;
}

void ResidentBuffer::ReadDevice(std::uint64_t guest, std::span<std::byte> out) {
    GuestMemory::AssertGpuLockHeld("heap mirror device read");
    Require(!Recorder::InCompletion(), "heap mirror device read inside a GPU completion: its write-back needs a recorded readback");
    auto staging = readbackBatch(guest, out.size());
    std::memcpy(out.data(), staging->Bytes().data(), out.size());
}

std::shared_ptr<Buffer> ResidentBuffer::readbackBatch(std::uint64_t guest, std::uint64_t bytes) {
    CommandBatch batch(context);
    RecordMemoryBarrier(context, batch.Handle(), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
    auto staging = RecordReadback(batch.Handle(), guest, bytes);
    RecordMemoryBarrier(context, batch.Handle(), VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
    batch.SubmitAndWait();
    return staging;
}

void ResidentBuffer::flush() {
    GuestRuns runs;
    {
        std::lock_guard lock(changedMutex);
        runs.swap(changed);
    }
    if (runs.empty()) return;
    try {
        std::sort(runs.begin(), runs.end());
        std::vector<VkBufferCopy> regions;
        std::uint64_t total = 0;
        std::uint64_t cursor = 0;
        for (const auto& [first, last] : runs) {
            for (auto at = std::max(first, cursor); at < last;) {
                const auto next = std::min(last, (at / BlockSize + 1) * BlockSize);
                if (slots[blockIndex(at)].host != nullptr) {
                    if (!regions.empty() && regions.back().dstOffset + regions.back().size == at - origin) regions.back().size += next - at;
                    else regions.push_back({total, at - origin, next - at});
                    total += next - at;
                }
                at = next;
            }
            cursor = std::max(cursor, last);
        }
        if (regions.empty()) return;
        auto staging = std::make_shared<Buffer>(context, static_cast<std::size_t>(total), VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
        for (const auto& region : regions) {
            for (VkDeviceSize done = 0; done < region.size;) {
                const auto at = origin + region.dstOffset + done;
                const auto length = std::min<VkDeviceSize>(region.size - done, BlockSize - (at - origin) % BlockSize);
                std::memcpy(staging->Bytes().data() + region.srcOffset + done, Block(at), static_cast<std::size_t>(length));
                done += length;
            }
        }
        recordTransfer(context, [&](VkCommandBuffer commands, std::vector<std::shared_ptr<void>>& kept) {
            context.Function<PFN_vkCmdCopyBuffer>("vkCmdCopyBuffer")(commands, staging->Handle(), buffer, static_cast<std::uint32_t>(regions.size()), regions.data());
            kept.push_back(staging);
            kept.push_back(shared_from_this());
        });
    } catch (...) {
        {
            std::lock_guard lock(changedMutex);
            changed.insert(changed.begin(), runs.begin(), runs.end());
        }
        auto& devices = Devices();
        std::lock_guard lock(devices.pendingMutex);
        devices.pending.push_back(weak_from_this());
        throw;
    }
}

void ResidentBuffer::FlushChanged(const Context& context) {
    auto& devices = Devices();
    std::vector<std::shared_ptr<ResidentMemory>> unzeroed;
    {
        std::lock_guard lock(devices.mutex);
        for (const auto& [key, held] : devices.memories) {
            if (std::get<0>(key) != context.device) continue;
            if (auto memory = held.lock(); memory != nullptr && !memory->Zeroed()) unzeroed.push_back(std::move(memory));
        }
    }
    for (auto& memory : unzeroed) recordTransfer(context, [&](VkCommandBuffer commands, std::vector<std::shared_ptr<void>>& kept) { memory->FillZero(commands, kept); });
    std::vector<std::weak_ptr<ResidentBuffer>> waiting;
    {
        std::lock_guard lock(devices.pendingMutex);
        waiting.swap(devices.pending);
    }
    for (std::size_t index = 0; index < waiting.size(); ++index) {
        const auto held = waiting[index].lock();
        if (held == nullptr) continue;
        if (held->context.device != context.device) {
            std::lock_guard lock(devices.pendingMutex);
            devices.pending.push_back(waiting[index]);
            continue;
        }
        try {
            held->flush();
        } catch (...) {
            std::lock_guard lock(devices.pendingMutex);
            devices.pending.insert(devices.pending.end(), waiting.begin() + static_cast<std::ptrdiff_t>(index) + 1, waiting.end());
            throw;
        }
    }
}

void ResidentBuffer::submit(const std::vector<VkSparseMemoryBind>& binds) {
    const bool ownQueue = context.sparseQueue != VK_NULL_HANDLE && context.sparseQueueMutex != nullptr;
    const VkSparseBufferMemoryBindInfo bufferBinds{buffer, static_cast<std::uint32_t>(binds.size()), binds.data()};
    VkBindSparseInfo bind{VK_STRUCTURE_TYPE_BIND_SPARSE_INFO};
    bind.bufferBindCount = 1;
    bind.pBufferBinds = &bufferBinds;
    const VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    VkFence fence = VK_NULL_HANDLE;
    if (const auto result = context.Function<PFN_vkCreateFence>("vkCreateFence")(context.device, &fenceInfo, nullptr, &fence); result != VK_SUCCESS) fail("vkCreateFence of a resident heap mirror bind", result);
    VkResult submitted = VK_SUCCESS;
    VkResult waited = VK_SUCCESS;
    {
        std::unique_lock<std::mutex> queueLock;
        std::unique_lock<GuestMemory::GpuMutexType> gpuLock;
        if (ownQueue) queueLock = std::unique_lock(*context.sparseQueueMutex);
        else gpuLock = std::unique_lock(GuestMemory::GpuMutex());
        submitted = context.Function<PFN_vkQueueBindSparse>("vkQueueBindSparse")(ownQueue ? context.sparseQueue : context.queue, 1, &bind, fence);
        if (submitted == VK_SUCCESS) waited = context.Function<PFN_vkWaitForFences>("vkWaitForFences")(context.device, 1, &fence, VK_TRUE, UINT64_MAX);
    }
    context.Function<PFN_vkDestroyFence>("vkDestroyFence")(context.device, fence, nullptr);
    if (submitted != VK_SUCCESS) fail("vkQueueBindSparse of a resident heap mirror", submitted);
    if (waited != VK_SUCCESS) fail("vkWaitForFences of a resident heap mirror bind", waited);
}

}
