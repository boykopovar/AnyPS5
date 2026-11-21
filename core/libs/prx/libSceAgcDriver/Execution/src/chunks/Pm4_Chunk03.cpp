#include "prx/libkernel/include/Api.hpp"
#include "prx/libSceAgcDriver/Execution/include/Pm4.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/PreciseWait.hpp"
#include "RdnaDecoder/RdnaInstructionDecoder.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <thread>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>

namespace AgcDriver::Pm4 {
namespace {
namespace {
namespace {
namespace {
std::uint32_t ReadDrawCount(const DrawParameters::IndirectDraw& indirect) {
    require(indirect.countIndirect && indirect.countAddress != 0, "indirect draw has no count address");
    std::uint32_t count = 0;
    const GuestMemory::ReadSiteScope site(GuestMemory::ReadSite::IndirectArguments);
    GuestMemory::Read(indirect.countAddress, std::as_writable_bytes(std::span(&count, 1)), 4);
    return count;
}

DrawParameters ResolveDraw(std::span<const std::uint32_t> packet, const QueueState& queue) {
    Validate(packet, 0);
    if (IndirectDrawOpcode((packet[0] >> 8u) & 0xffu)) return resolveIndirectDraw(packet, queue);
    if (((packet[0] >> 8u) & 0xffu) == 0x2d) {
        const auto offset = queue.userConfig.find(0x24a);
        require(offset != queue.userConfig.end(), "missing GE_INDX_OFFSET register");
        const auto firstVertex = offset->second;
        require(packet[1] == 0 || firstVertex <= std::numeric_limits<std::uint32_t>::max() - (packet[1] - 1u), "auto draw vertex range overflow");
        return {0, packet[1], 0, queue.instanceCount, packet[2] & 0x20u, false, firstVertex, 0};
    }
    const bool explicitAddress = ((packet[0] >> 8u) & 0xffu) == 0x27;
    require(explicitAddress || ((packet[0] >> 8u) & 0xffu) == 0x35, "expected indexed draw packet");
    require(queue.indexType <= 2, "unsupported index type");
    const std::uint32_t indexSize = queue.indexType == 0 ? 2 : queue.indexType == 1 ? 4 : 1;
    const auto base = explicitAddress ? address(packet[2], packet[3]) : queue.indexBase;
    const auto count = explicitAddress ? packet[4] : packet[3];
    require(base != 0 && base % indexSize == 0, "null or misaligned index base");
    const auto offset = explicitAddress ? 0 : static_cast<std::uint64_t>(packet[2]) * indexSize;
    require(offset <= std::numeric_limits<std::uint64_t>::max() - base, "index address overflow");
    const auto address = base + offset;
    const auto bytes = static_cast<std::uint64_t>(count) * indexSize;
    require(bytes <= std::numeric_limits<std::size_t>::max(), "index range size overflow");
    GuestMemory::CheckRange(reinterpret_cast<const void*>(address), static_cast<std::size_t>(bytes), indexSize);
    APS5_LOG_OUT_DEBUG("ResolveDraw context targetMask=0x%x shaderMask=0x%x indexCount=%u indexType=%u instances=%u", queue.context.contains(0x8e) ? queue.context.at(0x8e) : 0u, queue.context.contains(0x8f) ? queue.context.at(0x8f) : 0u, count, queue.indexType, queue.instanceCount);
    const auto indexOffset = queue.userConfig.find(0x24a);
    require(indexOffset != queue.userConfig.end(), "missing GE_INDX_OFFSET register");
    return {address, count, indexSize, queue.instanceCount, packet.back(), true, indexOffset->second, 0};
}

std::vector<std::uint32_t> ReadIndirectRegisters(std::span<const std::uint32_t> packet) {
    require(packet.size() == 5 && IndirectRegisterOpcode((packet[0] >> 8u) & 0xffu), "expected indirect register packet");
    // Named for the [hooksync] attribution (the read goes through the flush hook).
    const GuestMemory::ReadSiteScope site(GuestMemory::ReadSite::Registers);
    if (packet[3] != 0x80000000u) {
        std::vector<std::uint32_t> values(packet[4]);
        GuestMemory::Read(address(packet[1], packet[2]), std::as_writable_bytes(std::span(values)), 4);
        std::vector<std::uint32_t> pairs;
        pairs.reserve(values.size() * 2);
        for (std::size_t i = 0; i < values.size(); ++i) {
            pairs.push_back(packet[3] + static_cast<std::uint32_t>(i));
            pairs.push_back(values[i]);
        }
        return pairs;
    }
    std::vector<std::uint32_t> pairs(static_cast<std::size_t>(packet[4]) * 2);
    GuestMemory::Read(address(packet[1], packet[2]), std::as_writable_bytes(std::span(pairs)), 4);
    return pairs;
}

void NoteDispatchRegisters(std::span<const std::uint32_t> packet, QueueState& queue) {
    const bool direct = ((packet[0] >> 8u) & 0xffu) == 0x15u;
    queue.shader.insert_or_assign(0x200, packet.back());
    for (std::uint32_t i = 0; i < 3; ++i) {
        if (direct) queue.shader.insert_or_assign(0x201 + i, packet[1 + i]);
        else queue.shader.erase(0x201 + i);
    }
}

bool IndirectRegistersValid(std::span<const std::uint32_t> pairs) {
    for (std::size_t i = 0; i < pairs.size(); i += 2) {
        if (pairs[i] == 0xffffffffu || (pairs[i] & ~0x70000000u) > 0xffffu) return false;
    }
    return true;
}

void ExecuteIndirectRegisters(std::span<const std::uint32_t> packet, std::span<const std::uint32_t> pairs, QueueState& queue, bool skipInvalid) {
    const auto opcode = (packet[0] >> 8u) & 0xffu;
    require(packet.size() == 5 && IndirectRegisterOpcode(opcode), "expected indirect register packet");
    require(pairs.size() == static_cast<std::size_t>(packet[4]) * 2, "indirect register list does not match its packet");
    const auto usable = [&](std::uint32_t value) { return !skipInvalid || IndirectRegistersValid(std::span(&value, 1)); };
    for (std::size_t i = 0; i < pairs.size(); i += 2) {
        if (usable(pairs[i])) registerOffset(pairs[i]);
    }
    for (std::size_t i = 0; i < pairs.size(); i += 2) {
        if (usable(pairs[i])) writeRegister(queue, opcode, registerOffset(pairs[i]), pairs[i + 1]);
    }
    if (queue.savedContext.has_value() && TraceContextState()) {
        std::fprintf(stderr, "[context]   indirect 0x%x:", opcode);
        for (std::size_t i = 0; i < pairs.size(); i += 2) {
            if (usable(pairs[i])) std::fprintf(stderr, " %x", registerOffset(pairs[i]));
        }
        std::fprintf(stderr, "\n");
    }
}

namespace {

constexpr std::uint64_t IndexAttributesBytes = 16;

struct IndexAttributes {
    std::uint64_t address;
    std::array<std::uint32_t, 4> words;
};

IndexAttributes readIndexAttributes(std::span<const std::uint32_t> packet) {
    require(packet.size() == 4 && ((packet[0] >> 8u) & 0xffu) == 0x91, "expected INDEX_ATTRIBUTES_INDIRECT packet");
    require((packet[1] & 0xfu) == 0, "INDEX_ATTRIBUTES_INDIRECT indirect mode or reserved base bits are not implemented");
    const auto base = address(packet[1], packet[2]);
    const auto offset = static_cast<std::uint64_t>(packet[3] & 0xffffu) * IndexAttributesBytes;
    require(offset <= std::numeric_limits<std::uint64_t>::max() - base, "INDEX_ATTRIBUTES_INDIRECT record address overflow");
    IndexAttributes attributes{base + offset, {}};
    const GuestMemory::ReadSiteScope site(GuestMemory::ReadSite::IndirectArguments);
    GuestMemory::Read(attributes.address, std::as_writable_bytes(std::span(attributes.words)), IndexAttributesBytes);
    return attributes;
}

void executeIndexAttributes(const IndexAttributes& attributes, QueueState& queue) {
    if (attributes.words[3] > 3) {
        char what[160];
        std::snprintf(what, sizeof(what), "INDEX_ATTRIBUTES_INDIRECT record at 0x%llx has index type 0x%x (index-type modifiers are not implemented)", static_cast<unsigned long long>(attributes.address), attributes.words[3]);
        throw std::runtime_error(what);
    }
    queue.indexBase = address(attributes.words[0], attributes.words[1]);
    queue.indexBufferSize = attributes.words[2];
    queue.indexType = attributes.words[3];
}

}

void Execute(std::span<const std::uint32_t> packet, QueueState& queue) {
    const auto opcode = (packet[0] >> 8u) & 0xffu;
    switch (opcode) {
        case 0x10:
            switch ((packet[0] >> 2u) & 0x3fu) {
                case 0: return;
                case 0x09: queue = std::move(*std::make_unique<QueueState>()); return;
                case 0x0b: queue.markers.emplace_back(reinterpret_cast<const char*>(packet.data() + 1)); return;
                case 0x0c:
                    if (!queue.markers.empty()) queue.markers.pop_back();
                    return;
                case 0x1a:
                    if (TraceContextState()) std::fprintf(stderr, "[context] op %u: %zu registers, saved %d, cb0 %x info %x mask %x\n", packet[1], queue.context.size(), queue.savedContext.has_value() ? 1 : 0, queue.context.contains(0x318) ? queue.context.at(0x318) : 0u, queue.context.contains(0x31c) ? queue.context.at(0x31c) : 0u, queue.context.contains(0x8e) ? queue.context.at(0x8e) : 0u);
                    APS5_LOG_OUT_DEBUG("CONTEXT_STATE operation=%u targetMaskBefore=0x%x shaderMaskBefore=0x%x", packet[1], queue.context.contains(0x8e) ? queue.context.at(0x8e) : 0u, queue.context.contains(0x8f) ? queue.context.at(0x8f) : 0u);
                    switch (packet[1]) {
                        case 0: queue.ClearContext(); break;
                        case 1: case 3:
                            require(!queue.savedContext.has_value(), "context state is already pushed");
                            queue.PushContext();
                            if (packet[1] == 3) queue.ClearContext();
                            break;
                        case 2:
                            require(queue.savedContext.has_value(), "context state has not been pushed");
                            queue.PopContext();
                            break;
                    }
                    APS5_LOG_OUT_DEBUG("CONTEXT_STATE done operation=%u targetMaskAfter=0x%x shaderMaskAfter=0x%x", packet[1], queue.context.contains(0x8e) ? queue.context.at(0x8e) : 0u, queue.context.contains(0x8f) ? queue.context.at(0x8f) : 0u);
                    return;
                default: throw std::runtime_error("custom packet requires driver execution");
            }
        case 0x11:
            ((packet[0] & 2u) == 0 ? queue.drawIndirectBase : queue.dispatchIndirectBase) = address(packet[2], packet[3]);
            return;
        case 0x12:
            APS5_LOG_OUT_DEBUG("CLEAR_STATE targetMaskBefore=0x%x shaderMaskBefore=0x%x", queue.context.contains(0x8e) ? queue.context.at(0x8e) : 0u, queue.context.contains(0x8f) ? queue.context.at(0x8f) : 0u);
            queue.ClearContext(); return;
        case 0x13: queue.indexBufferSize = packet[1]; return;
        case 0x20: {
            const auto operation = (packet[1] >> 16u) & 7u;
            queue.predication = {operation == 0 ? 0 : address(packet[2], packet[3]), operation, (packet[1] & 0x100u) != 0, (packet[1] & 0x1000u) != 0};
            return;
        }
        case 0x3f: throw std::runtime_error(std::string(UnsupportedReason(packet[0])));
        case 0x26: queue.indexBase = address(packet[1], packet[2]); return;
        case 0x2a: queue.indexType = packet[1]; return;
        case 0x91: executeIndexAttributes(readIndexAttributes(packet), queue); return;
        case 0x2f: queue.instanceCount = packet[1]; return;
        case 0x46:
            if (!SampleControl(packet)) throw std::runtime_error("packet requires driver execution");
            queue.sampleDumpBlocks = SampleControlBlocks(packet);
            return;
        case 0x63: case 0x64: case 0x9f:
            ExecuteIndirectRegisters(packet, ReadIndirectRegisters(packet), queue);
            return;
        case 0x59: break;
        case 0x3c: case 0x93: {
            // The waited-on value is written by the CPU or another queue; poll it like the CP would.
            const auto start = std::chrono::steady_clock::now();
            bool warned = false;
            while (!WaitSatisfied(packet)) {
                PreciseSleepUs(50);
                if (!warned && std::chrono::steady_clock::now() - start > std::chrono::seconds(5)) {
                    warned = true;
                    std::fprintf(stderr, "[gpu] WAIT_REG_MEM at 0x%llx still waiting after 5s (function %u ref 0x%x mask 0x%x)\n",
                                 static_cast<unsigned long long>(address(packet[2] & (WaitAwaitedBytes(packet) == 8 ? ~7u : ~3u), packet[3])), packet[1] & 7u, packet[4], packet[5]);
                }
            }
            return;
        }
        case 0x39: {
            const auto semaphore = address(packet[1], packet[2]);
            auto* word = reinterpret_cast<std::uint64_t*>(semaphore);
            std::atomic_ref<std::uint64_t> shared(*word);
            const auto prepare = [&] {
                GuestMemory::GuardRead(semaphore, sizeof(*word));
                GuestMemory::FlushGpuWrites(semaphore, sizeof(*word));
                GuestMemory::CheckRange(word, sizeof(*word), std::atomic_ref<std::uint64_t>::required_alignment, true);
            };
            if ((packet[3] >> 29u) == 6u) {
                prepare();
                const bool set = (packet[3] & 0x100000u) != 0;
                GuestMemory::StoreOwnBytes(semaphore, sizeof(*word), [&] {
                    if (set) shared.store(1, std::memory_order_release);
                    else shared.fetch_add(1, std::memory_order_acq_rel);
                });
                return;
            }
            const auto start = std::chrono::steady_clock::now();
            bool warned = false;
            for (;;) {
                {
                    const GuestMemory::ReadSiteScope site(GuestMemory::ReadSite::Wait);
                    prepare();
                }
                auto value = shared.load(std::memory_order_acquire);
                if (value != 0) {
                    bool taken = false;
                    GuestMemory::StoreOwnBytes(semaphore, sizeof(*word), [&] {
                        taken = shared.compare_exchange_strong(value, value - 1, std::memory_order_acq_rel);
                    });
                    if (taken) return;
                    continue;
                }
                PreciseSleepUs(50);
                if (!warned && std::chrono::steady_clock::now() - start > std::chrono::seconds(5)) {
                    warned = true;
                    std::fprintf(stderr, "[gpu] MEM_SEMAPHORE wait at 0x%llx still waiting after 5s\n", static_cast<unsigned long long>(semaphore));
                }
            }
        }
        case 0x49: {
            // Earlier work has drained by the time the driver reaches this packet, so the end-of-pipe
            // write can happen immediately.
            const auto dataSelect = packet[2] >> 29u;
            const auto destination = address(packet[3], packet[4]);
            if (dataSelect == 0 || destination == 0) return;
            const auto value = ReleaseMemValue(packet);
            GuestMemory::Write(destination, std::as_bytes(std::span(&value, 1)).first(dataSelect == 1 ? 4 : 8), dataSelect == 1 ? 4 : 8);
            return;
        }
        case 0x51: {
            const auto offset = packet[1];
            require(queue.context.contains(offset), "CONTEXT_REG_RMW register has no known value");
            const auto value = (queue.context.at(offset) & ~packet[2]) | (packet[3] & packet[2]);
            writeRegister(queue, 0x69, offset, value);
            return;
        }
        case 0x69: case 0x76: case 0x79: case 0x7a: {
            if (IsTagMarker(packet)) return;
            const auto offset = registerOffset(packet[1]);
            for (std::size_t i = 2; i < packet.size(); ++i) writeRegister(queue, opcode, offset + static_cast<std::uint32_t>(i - 2), packet[i]);
            return;
        }
        case 0x81:
            std::copy(packet.begin() + 2, packet.end(), queue.WritableConstantRam().begin() + packet[1] / 4);
            return;
        case 0x83: {
            const auto dump = DecodeConstantRamDump(packet);
            GuestMemory::Write(dump.destination, std::as_bytes(queue.ConstantRam().subspan(dump.offset / 4, dump.words)), 4);
            return;
        }
        case 0x45: {
            const auto write = DecodeConditionalWrite(packet);
            std::uint32_t value = 0;
            if ((packet[1] & 0x10u) != 0) {
                const GuestMemory::ReadSiteScope site(GuestMemory::ReadSite::Wait);
                GuestMemory::Read(write.compared, std::as_writable_bytes(std::span(&value, 1)), 4);
            } else {
                const auto base = registerBankBase(packet[2]);
                const auto& bank = base == ShaderRegisterBase ? queue.shader : base == ContextRegisterBase ? queue.context : queue.userConfig;
                require(base != 0 && bank.contains(packet[2] - base), "COND_WRITE poll register has no known value");
                value = bank.at(packet[2] - base);
            }
            if (((packet[1] >> 8u) & 3u) == 0) {
                if (waitCompares(packet, false, value)) {
                    const auto base = registerBankBase(packet[6]);
                    writeRegister(queue, base == ShaderRegisterBase ? 0x76u : base == ContextRegisterBase ? 0x69u : 0x79u, packet[6] - base, write.value);
                }
                return;
            }
            if (waitCompares(packet, false, value)) GuestMemory::Write(write.destination, std::as_bytes(std::span(&write.value, 1)), 4);
            return;
        }
        case 0x37: {
            if (((packet[1] >> 8u) & 0xfu) == 0) {
                const bool repeat = (packet[1] & 0x10000u) != 0;
                for (std::size_t i = 4; i < packet.size(); ++i) writeRegister(queue, 0x79, packet[2] - 0xc000u + (repeat ? 0u : static_cast<std::uint32_t>(i - 4)), packet[i]);
                return;
            }
            const auto destination = address(packet[2], packet[3]);
            if ((packet[1] & 0x10000u) != 0) {
                for (const auto& value : packet.subspan(4)) GuestMemory::Write(destination, std::as_bytes(std::span(&value, 1)), 4);
            } else GuestMemory::Write(destination, std::as_bytes(packet.subspan(4)), 4);
            return;
        }
        case 0x40: {
            if (CopyDataToRegister(packet)) {
                std::array<std::uint32_t, 2> values{};
                const auto count = copyDataCount(packet);
                const GuestMemory::ReadSiteScope site(GuestMemory::ReadSite::Registers);
                GuestMemory::Read(address(packet[2], packet[3]), std::as_writable_bytes(std::span(values).first(count)), 4);
                for (std::uint32_t i = 0; i < count; ++i) writeRegister(queue, 0x69, packet[4] - ContextRegisterBase + i, values[i]);
                return;
            }
            const auto destination = CopyDataDestination(packet) == CopyDataSelectGds ? GdsAddress() + packet[4] : address(packet[4], packet[5]);
            const auto data = copyDataSource(packet, queue, CopyDataBytes(packet));
            GuestMemory::CheckRange(reinterpret_cast<void*>(destination), data.size(), 1, true);
            GuestMemory::Write(destination, data);
            return;
        }
        case 0x28: return;
        case 0x50: {
            const std::size_t bytes = DmaBytes(packet);
            if (bytes == 0 || dmaDestination(packet) == DmaDestinationNowhere) return;
            auto data = dmaSourceBytes(packet);
            const auto destination = dmaDestination(packet) == DmaSelectGds ? GdsAddress() + packet[4] : address(packet[4], packet[5]);
            GuestMemory::CheckRange(reinterpret_cast<void*>(destination), bytes, 1, true);
            GuestMemory::Write(destination, data);
            return;
        }
        default: throw std::runtime_error("packet requires driver execution");
    }
}

}
}
}
}
}
