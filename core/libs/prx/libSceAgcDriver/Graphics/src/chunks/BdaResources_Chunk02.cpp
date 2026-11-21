#include "prx/libSceAgcDriver/Graphics/include/BdaResources.hpp"
#include "prx/libSceAgcDriver/Execution/include/Mutex.hpp"
#include "prx/libSceAgcDriver/Graphics/include/MultisampleTarget.hpp"
#include "ImageTableAbi.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <list>
#include <mutex>
#include <iomanip>
#include <span>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace AgcDriver::Graphics {
namespace {
namespace {
VkDescriptorBufferInfo BdaResources::Fault() const {
    return {faultChunk->buffer->Handle(), faultOffset, faultChunk->slotBytes};
}

namespace {
std::atomic<bool> loopGuardTripped{false};
}

bool LoopGuardTripped() {
    return loopGuardTripped.load(std::memory_order_relaxed);
}

void BdaResources::CheckFault() const {
    checkFault(*this);
}

void BdaResources::CheckUse(const BdaResources* record) const {
    const auto& checked = record != nullptr ? *record : *this;
    if (checked.faultChunk == nullptr) return;
    const struct Cleared {
        std::span<std::byte> bytes;
        ~Cleared() {
            if (!faultSlotClean(bytes)) clearFaultSlot(bytes);
        }
    } cleared{checked.faultBytes()};
    checkFault(checked);
}

void BdaResources::checkFault(const BdaResources& record) const {
    if (record.faultChunk == nullptr) return;
    if (!reportOnly) markWrittenPages(record);
    ShaderRecompiler::BdaAbi::Fault report{};
    std::memcpy(&report, record.faultBytes().data(), sizeof(report));
    if (report.state == ShaderRecompiler::BdaAbi::FaultState::Empty) {
        Require(static_cast<std::uint32_t>(report.reason) == 0 && report.address == 0 && report.bytes == 0 && report.stage == 0 && report.instruction == 0 && report.claim == 0, "BDA fault record has data without publication");
        if (!reportOnly) checkSrtGuard(record);
        return;
    }
    Require(report.state == ShaderRecompiler::BdaAbi::FaultState::Ready && (report.claim & ~(ShaderRecompiler::BdaAbi::FaultClaimed | ShaderRecompiler::BdaAbi::FaultClaimOtherFaults)) == 0, "incomplete or invalid BDA fault record");
    Require(report.reason != ShaderRecompiler::BdaAbi::FaultReason::InvalidRectangle, "rect-list requires finite nondegenerate axis-aligned positions with equal positive W");
    if (report.reason == ShaderRecompiler::BdaAbi::FaultReason::LoopLimit) {
        loopGuardTripped.store(true, std::memory_order_relaxed);
        std::fprintf(stderr, "[gpu] loop guard: the loop exit at pc 0x%x of shader 0x%llx ran past %u evaluations\n", report.instruction, static_cast<unsigned long long>(report.address), report.bytes);
        std::memset(record.faultBytes().data(), 0, record.faultBytes().size());
        return;
    }
    if (report.reason == ShaderRecompiler::BdaAbi::FaultReason::ImageTable) {
        const auto table = std::find_if(imageTables.begin(), imageTables.end(), [&](const ImageTableFaults& candidate) { return candidate.stage == report.stage; });
        const auto* tables = table != imageTables.end() ? &*table : imageTables.size() == 1 ? &imageTables.front() : nullptr;
        std::ostringstream message;
        message << "image table: shader 0x" << std::hex << (tables != nullptr ? tables->shader : 0u) << std::dec << " stage " << report.stage << " pc 0x" << std::hex << report.instruction << " selected the descriptor at 0x" << report.address << std::dec;
        if (tables != nullptr && report.bytes < tables->poison.size()) {
            const auto& poison = tables->poison[report.bytes];
            if (poison.dwordCount != 0) {
                message << " (words";
                for (std::uint32_t index = 0; index < poison.dwordCount && index < poison.words.size(); ++index) message << ' ' << std::hex << std::setw(8) << std::setfill('0') << poison.words[index];
                message << std::dec << ")";
            }
            message << ": " << ShaderRecompiler::ImageTableAbi::PoisonReasonName(static_cast<ShaderRecompiler::ImageTableAbi::PoisonReason>(poison.reason));
            if (report.bytes < tables->messages.size() && tables->messages[report.bytes] != nullptr && !tables->messages[report.bytes]->empty()) message << ": " << *tables->messages[report.bytes];
        } else {
            message << ": poison entry " << report.bytes;
        }
        throw std::runtime_error(message.str());
    }
    std::ostringstream message;
    if (report.reason == ShaderRecompiler::BdaAbi::FaultReason::UnlinkedCall) {
        message << "shader stage " << report.stage << " called address 0x" << std::hex << report.address << " at program counter 0x" << report.instruction << ", which no linked callee of that call matches";
        throw std::runtime_error(message.str());
    }
    if (report.reason == ShaderRecompiler::BdaAbi::FaultReason::WindowLaneSpill) {
        message << "shader stage " << report.stage << ": a relative write stored " << report.bytes << " in lane " << (report.address >> 32u) << " of vector window slot " << (report.address & 0xffffffffu) << ", which holds a scalar spilled with v_writelane_b32 that a later v_readlane_b32 was replaced with";
        throw std::runtime_error(message.str());
    }
    message << "BDA access failed: address=0x" << std::hex << report.address << " instruction=0x" << report.instruction << std::dec << " bytes=" << report.bytes << " stage=" << report.stage << " reason=" << static_cast<std::uint32_t>(report.reason);
    if (report.reason == ShaderRecompiler::BdaAbi::FaultReason::Permission && table != nullptr) {
        const auto bytes = table->Bytes();
        ShaderRecompiler::BdaAbi::Header header{};
        std::memcpy(&header, bytes.data(), sizeof(header));
        for (std::uint32_t index = 0; index < header.count; ++index) {
            ShaderRecompiler::BdaAbi::Range range{};
            std::memcpy(&range, bytes.data() + sizeof(header) + index * sizeof(range), sizeof(range));
            if (report.address < range.begin || report.address >= range.end) continue;
            message << std::hex << "; the store hit 0x" << range.begin << "+0x" << range.end - range.begin << ", read-only in the BDA table: stores through GPU-selected descriptors and FLAT/GLOBAL addresses reach only writable ranges imported in place, not ones served by a mirror or a copy (past APS5_HOST_IMPORT_MIB, or refused by the driver)";
            break;
        }
    }
    if (report.reason == ShaderRecompiler::BdaAbi::FaultReason::Unaligned) {
        if (ShaderRecompiler::BdaAbi::OtherFaults(report) == 0) throw BdaUnalignedAccess(message.str(), report.instruction);
        message << "; a fault of another kind was recorded in the same dispatch";
    }
    throw std::runtime_error(message.str());
}

}

namespace AgcDriver::Graphics {

void BdaResources::SetImageTables(std::vector<ImageTableFaults> tables, std::vector<std::pair<std::uint64_t, std::uint64_t>> ranges) {
    imageTables = std::move(tables);
    imageTableRanges = std::move(ranges);
}

void BdaResources::SetSrtGuards(std::vector<SrtGuardFaults> guards) {
    srtGuards = std::move(guards);
}

void BdaResources::checkSrtGuard(const BdaResources& faults) const {
    namespace Abi = ShaderRecompiler::BdaAbi;
    std::uint32_t guard = 0;
    std::memcpy(&guard, faults.faultBytes().data() + Abi::SrtGuardWord * sizeof(std::uint32_t), sizeof(guard));
    if (guard == 0) return;
    const auto stage = guard >> Abi::SrtGuardStageShift;
    const auto record = guard & ((1u << Abi::SrtGuardStageShift) - 1u);
    const auto found = std::find_if(srtGuards.begin(), srtGuards.end(), [&](const SrtGuardFaults& candidate) { return candidate.stage == stage; });
    const auto* guards = found != srtGuards.end() ? &*found : srtGuards.size() == 1 ? &srtGuards.front() : nullptr;
    Require(guards != nullptr && record + 2u < guards->flattenedSrt.size(), "a guarded SRT read faulted without its poison record");
    const auto& words = guards->flattenedSrt;
    const auto address = static_cast<std::uint64_t>(words[record + 1u]) | (static_cast<std::uint64_t>(words[record + 2u]) << 32u);
    std::ostringstream message;
    message << "BDA access failed: address=0x" << std::hex << address << " instruction=0x" << words[record] << std::dec << " bytes=4 stage=" << stage << " reason=" << static_cast<std::uint32_t>(Abi::FaultReason::Unmapped);
    throw std::runtime_error(message.str());
}

void BdaResources::markWrittenPages(const BdaResources& record) const {
    namespace Abi = ShaderRecompiler::BdaAbi;
    auto* words = reinterpret_cast<std::uint32_t*>(record.faultBytes().data());
    Require(words[Abi::WrittenOverflowWord] == 0, "more than " + std::to_string(Abi::WrittenPageSlots) + " chunks of " + std::to_string(Abi::WrittenChunkPages) + " pages stored to through the BDA table in one use are not implemented");
    const auto count = words[Abi::WrittenCountWord];
    if (count == 0) return;
    Require(Abi::ForEachWrittenRange(words, [&](std::uint64_t begin, std::size_t bytes) {
        const auto end = begin + bytes;
        for (const auto& [base, size] : imageTableRanges) {
            if (size != 0 && begin < base + size && base < end) {
                std::ostringstream message;
                message << "image table at 0x" << std::hex << base << " is written by its own dispatch";
                Require(false, message.str());
            }
        }
        GuestMemory::MarkPossiblyWritten(begin, bytes);
    }), "invalid BDA written-page record");
    for (std::uint32_t claim = 0; claim < count; ++claim) {
        const auto slot = words[Abi::WrittenListWord + claim];
        words[Abi::WrittenSlotsWord + 2 * slot] = 0;
        words[Abi::WrittenSlotsWord + 2 * slot + 1] = 0;
    }
    words[Abi::WrittenCountWord] = 0;
}

}
}
}
