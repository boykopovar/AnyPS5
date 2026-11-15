#include "prx/libSceAgcDriver/Graphics/include/TextureService.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "BindlessImageAbi.hpp"
#include "Recompiler.hpp"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace AgcDriver::Graphics {

namespace {

namespace Abi = ShaderRecompiler::DeviceImageTableAbi;
namespace Heap = ShaderRecompiler::BindlessImageAbi;

std::atomic<std::uint64_t> oldPathNanoseconds{0};

std::uint64_t Elapsed(std::chrono::steady_clock::time_point started) {
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - started).count());
}

std::uint64_t EnvironmentNumber(const char* name, std::uint64_t fallback) {
    const char* value = std::getenv(name);
    if (value == nullptr || *value == '\0') return fallback;
    char* end = nullptr;
    const auto parsed = std::strtoull(value, &end, 10);
    Require(end != nullptr && *end == '\0', std::string(name) + " is not a decimal number: " + value);
    return parsed;
}

double Milliseconds(std::uint64_t nanoseconds) {
    return static_cast<double>(nanoseconds) / 1.0e6;
}

constexpr std::array<const char*, static_cast<std::size_t>(BindlessRefusalKind::Count)> RefusalKindNames{"storage", "msaa", "compare", "unmapped", "no-mode", "sampler", "interface", "other"};

static_assert(TextureService::MaxRejectionDetails + 1u < Abi::FaultFlag, "a fault record must be able to name every retained rejection");

std::string RejectionDetail(const DeviceImageKeys::Key& key, const std::string& reason) {
    std::string message = "(words";
    for (std::uint32_t word = 0; word < Abi::DescriptorWords; ++word) {
        char text[12];
        std::snprintf(text, sizeof(text), " %08x", key[word]);
        message += text;
    }
    return message + "): rejected by the driver: " + reason;
}

std::string NormalizedReason(std::string_view reason) {
    std::string normalized;
    for (std::size_t index = 0; index < reason.size();) {
        if (reason.substr(index, 2) == "0x") {
            index += 2u;
            while (index < reason.size() && std::isxdigit(static_cast<unsigned char>(reason[index])) != 0) ++index;
            normalized += '#';
            continue;
        }
        if (std::isdigit(static_cast<unsigned char>(reason[index])) != 0) {
            while (index < reason.size() && std::isdigit(static_cast<unsigned char>(reason[index])) != 0) ++index;
            normalized += '#';
            continue;
        }
        normalized += reason[index++];
    }
    return normalized;
}

}

BindlessRefusalKind ClassifyBindlessRefusal(std::string_view reason) {
    const auto has = [&](std::string_view text) { return reason.find(text) != std::string_view::npos; };
    if (has("storage image read or written through the bindless heap")) return BindlessRefusalKind::Storage;
    if (has("multisampled image read through the bindless heap")) return BindlessRefusalKind::Multisample;
    if (has("comparison sampling of a color texture")) return BindlessRefusalKind::ColorCompare;
    if (has("is not mapped")) return BindlessRefusalKind::Unmapped;
    if (has("matches no runtime mode")) return BindlessRefusalKind::NoMode;
    if (has("which no compiled program registered")) return BindlessRefusalKind::Interface;
    if (has("sampler") || has("ANISO_OVERRIDE")) return BindlessRefusalKind::Sampler;
    return BindlessRefusalKind::Other;
}

std::size_t TextureService::KeyHash::operator()(const DeviceImageKeys::Key& key) const noexcept {
    return Abi::Hash(key);
}

std::uint64_t BindlessOldPathNanoseconds() {
    return oldPathNanoseconds.load(std::memory_order_relaxed);
}

void AddBindlessOldPathNanoseconds(std::uint64_t nanoseconds) {
    oldPathNanoseconds.fetch_add(nanoseconds, std::memory_order_relaxed);
}

std::vector<std::pair<std::uint64_t, std::uint64_t>> BindlessReadableRanges(const ShaderRecompiler::RecompileResult& program) {
    std::vector<std::pair<std::uint64_t, std::uint64_t>> ranges;
    if (!program.bindlessHeap) return ranges;
    for (const auto& [base, bytes] : program.imageTableRanges) {
        if (bytes != 0u) ranges.emplace_back(base, bytes);
    }
    for (const auto& table : program.bindlessTables) {
        if (table.records != 0u) ranges.emplace_back(table.base, static_cast<std::uint64_t>(table.records - 1u) * table.stride + table.dwords * 4u);
    }
    return ranges;
}

void PrimeBindlessTables(const Context& context, std::span<const BindlessTableRegistration> tables) {
    if (context.textureService == nullptr) return;
    for (const auto& registration : tables) static_cast<void>(context.textureService->Register(registration));
}

void ServeBindlessTables(const Context& context, Recorder& recorder, std::span<const BindlessTableRegistration> tables) {
    if (context.textureService == nullptr) return;
    std::vector<std::uint64_t> served;
    served.reserve(tables.size());
    for (const auto& registration : tables) served.push_back(context.textureService->Register(registration));
    context.textureService->Serve(served, recorder);
}

bool BindlessStatsRequested() {
    static const bool requested = std::getenv("APS5_BINDLESS_STATS") != nullptr || std::getenv("APS5_PROFILE_DRAW") != nullptr;
    return requested;
}

TextureService::Options TextureService::OptionsFromEnvironment(std::uint64_t defaultBudgetBytes) {
    Options options;
    const char* lazy = std::getenv("APS5_BINDLESS_LAZY");
    options.lazy = lazy != nullptr && std::string_view(lazy) == "1";
    options.commitChunk = static_cast<std::uint32_t>(std::max<std::uint64_t>(1u, EnvironmentNumber("APS5_BINDLESS_COMMIT_CHUNK", options.commitChunk)));
    options.budgetBytes = EnvironmentNumber("APS5_BINDLESS_BUDGET_MIB", defaultBudgetBytes >> 20u) << 20u;
    options.idleSubmits = EnvironmentNumber("APS5_BINDLESS_IDLE_SUBMITS", options.idleSubmits);
    return options;
}

TextureService::TextureService(BindlessTable& table, std::shared_ptr<BindlessResolver> resolver, std::shared_ptr<BindlessMemory> memory, Options options) : table(table), resolver(std::move(resolver)), memory(std::move(memory)), options(options) {
    Require(this->resolver != nullptr && this->memory != nullptr, "the texture service needs a resolver and a guest memory view");
    mappingSerial = this->memory->MappingSerial();
    budgetBytes = this->options.budgetBytes;
    if (options.worker) worker = std::thread([this] { workerLoop(); });
}

TextureService::~TextureService() {
    {
        std::lock_guard lock(mutex);
        stopping = true;
    }
    changed.notify_all();
    if (worker.joinable()) worker.join();
}

std::uint64_t TextureService::Register(const BindlessTableRegistration& registration) {
    Require(registration.records != 0 && registration.dwords != 0u && registration.dwords <= Abi::DescriptorWords && registration.stride >= registration.dwords * 4u && registration.stride % 4u == 0u && !registration.uses.empty(), "a bindless table registration needs records, 1-8 T# dwords, a stride of at least those dwords in whole dwords and at least one use");
    std::lock_guard lock(mutex);
    if (const auto found = ids.find(registration); found != ids.end()) return found->second;
    const auto id = nextId++;
    auto& tracked = tables[id];
    tracked.id = id;
    tracked.registration = registration;
    tracked.span = static_cast<std::uint64_t>(registration.records - 1u) * registration.stride + registration.dwords * 4u;
    tracked.firstPage = registration.base & ~(PageBytes - 1u);
    tracked.registeredAt = static_cast<std::uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
    tracked.evictionEpoch = evictionEpoch;
    ids.emplace(registration, id);
    ++totals.registered;
    if (options.lazy) {
        tracked.state = TableState::Done;
        return id;
    }
    queue.push_back(id);
    changed.notify_all();
    return id;
}

std::vector<BindlessTableRegistration> TextureService::Registrations() const {
    std::lock_guard lock(mutex);
    std::vector<BindlessTableRegistration> result;
    result.reserve(tables.size());
    for (const auto& [id, tracked] : tables) result.push_back(tracked.registration);
    return result;
}

bool TextureService::Primed(std::uint64_t id) const {
    std::lock_guard lock(mutex);
    const auto found = tables.find(id);
    return found != tables.end() && found->second.state == TableState::Done;
}

bool TextureService::WaitPrimed(std::uint64_t id, std::uint32_t milliseconds) {
    Lock lock(mutex);
    return changed.wait_for(lock, std::chrono::milliseconds(milliseconds), [&] {
        const auto found = tables.find(id);
        return found == tables.end() || found->second.state == TableState::Done;
    });
}

std::vector<DeviceImageKeys::Key> TextureService::recordKeys(const Tracked& tracked, std::uint32_t record) const {
    if (tracked.words.size() < (static_cast<std::size_t>(record) + 1u) * Abi::DescriptorWords) return {};
    const auto words = std::span<const std::uint32_t>(tracked.words).subspan(static_cast<std::size_t>(record) * Abi::DescriptorWords, Abi::DescriptorWords);
    std::vector<DeviceImageKeys::Key> keys;
    keys.reserve(tracked.registration.uses.size());
    const auto descriptor = std::span<const std::uint32_t, Abi::DescriptorWords>(words.data(), Abi::DescriptorWords);
    for (const auto& use : tracked.registration.uses) keys.push_back(Heap::MakeKey(descriptor, use.interfaceId, use.samplerClass, use.sampler));
    return keys;
}

BindlessPrepared TextureService::prepare(const DeviceImageKeys::Key& key) {
    if (Abi::NullDescriptor(key[0], key[1])) {
        BindlessPrepared null;
        null.key = key;
        null.settled = DeviceImageValue{};
        return null;
    }
    try {
        auto prepared = resolver->Prepare(key);
        prepared.key = key;
        return prepared;
    } catch (const BindlessKeyRefusal& error) {
        BindlessPrepared refused;
        refused.key = key;
        refused.refusal = error.what();
        refused.permanent = true;
        return refused;
    } catch (const std::runtime_error& error) {
        BindlessPrepared refused;
        refused.key = key;
        refused.refusal = error.what();
        return refused;
    }
}

void TextureService::readTable(Tracked& tracked) {
    const auto started = std::chrono::steady_clock::now();
    const auto& registration = tracked.registration;
    const auto pages = static_cast<std::size_t>((registration.base + tracked.span - tracked.firstPage + PageBytes - 1u) / PageBytes);
    std::vector<std::uint32_t> words(static_cast<std::size_t>(registration.records) * Abi::DescriptorWords, 0u);
    const auto generation = memory->Collect(registration.base, static_cast<std::size_t>(tracked.span));
    bool mapped = registration.dwords == Abi::DescriptorWords && registration.stride == Abi::DescriptorWords * 4u && memory->Read(registration.base, words);
    if (!mapped) {
        std::fill(words.begin(), words.end(), 0u);
        for (std::uint32_t record = 0; record < registration.records; ++record) {
            const auto slot = std::span<std::uint32_t>(words).subspan(static_cast<std::size_t>(record) * Abi::DescriptorWords, registration.dwords);
            if (memory->Read(registration.base + static_cast<std::uint64_t>(record) * registration.stride, slot)) mapped = true;
            else std::fill(slot.begin(), slot.end(), 0u);
        }
    }
    Tracked shape;
    shape.registration = registration;
    shape.words = std::move(words);
    std::vector<DeviceImageKeys::Key> wanted;
    {
        std::set<DeviceImageKeys::Key> seen;
        for (std::uint32_t record = 0; record < registration.records; ++record) {
            for (const auto& key : recordKeys(shape, record)) {
                if (seen.insert(key).second) wanted.push_back(key);
            }
        }
    }
    {
        std::lock_guard lock(mutex);
        std::erase_if(wanted, [&](const DeviceImageKeys::Key& key) { return entries.contains(key); });
    }
    std::vector<BindlessPrepared> prepared;
    prepared.reserve(wanted.size());
    for (const auto& key : wanted) prepared.push_back(prepare(key));
    std::lock_guard lock(mutex);
    tracked.words = std::move(shape.words);
    tracked.pageGenerations.assign(pages, generation);
    tracked.oldest = generation;
    tracked.pending = std::move(prepared);
    tracked.unmapped = !mapped;
    tracked.perUseEpoch = ~std::uint64_t{0};
    if (tracked.state != TableState::Prepared) ++preparedTables;
    tracked.state = TableState::Prepared;
    tracked.primerNanoseconds += Elapsed(started);
    totals.primedRecords += registration.records;
    totals.primedKeys += tracked.pending.size();
    changed.notify_all();
}

void TextureService::workerLoop() {
    while (true) {
        Lock lock(mutex);
        changed.wait_for(lock, std::chrono::milliseconds(4), [&] { return stopping || !queue.empty(); });
        if (stopping) return;
        if (!queue.empty()) {
            const auto id = queue.front();
            queue.pop_front();
            auto found = tables.find(id);
            if (found == tables.end() || found->second.state != TableState::Queued) continue;
            found->second.state = TableState::Reading;
            lock.unlock();
            readTable(found->second);
            lock.lock();
            ++totals.workerPrepared;
            continue;
        }
        takingMisses = true;
        lock.unlock();
        auto misses = table.TakeMisses();
        lock.lock();
        const auto fresh = unresolved(misses);
        lock.unlock();
        std::vector<BindlessPrepared> prepared;
        for (const auto& miss : fresh) prepared.push_back(prepare(miss.key));
        lock.lock();
        totals.misses += misses.size();
        missPending.insert(missPending.end(), std::make_move_iterator(prepared.begin()), std::make_move_iterator(prepared.end()));
        takingMisses = false;
        changed.notify_all();
        const bool work = !missPending.empty() || preparedTables != 0;
        lock.unlock();
        if (!work) continue;
        auto& gpu = GuestMemory::GpuMutex();
        if (!gpu.try_lock()) continue;
        try {
            if (auto* recorder = Recorder::Active(); recorder != nullptr) {
                lock.lock();
                bool committed = false;
                for (auto& [id, tracked] : tables) {
                    if (tracked.state != TableState::Prepared) continue;
                    const auto before = tracked.pending.size();
                    commitTable(tracked, *recorder, lock, options.commitChunk);
                    totals.workerCommitted += before - tracked.pending.size();
                    committed = true;
                    break;
                }
                if (!committed && !missPending.empty()) {
                    auto prepared = std::exchange(missPending, {});
                    totals.missesResolved += prepared.size();
                    totals.workerCommitted += prepared.size();
                    commitPrepared(prepared, *recorder, lock);
                }
                lock.unlock();
            }
        } catch (const std::exception& error) {
            std::fprintf(stderr, "[bindless] texture service worker: %s\n", error.what());
        }
        gpu.unlock();
    }
}

BindlessResolution TextureService::settle(const BindlessPrepared& prepared) {
    if (!prepared.refusal.empty()) {
        BindlessResolution refused;
        refused.value.fault = 1u;
        return refused;
    }
    if (prepared.settled.has_value()) {
        BindlessResolution settled;
        settled.value = *prepared.settled;
        return settled;
    }
    return resolver->Commit(prepared);
}

std::uint32_t TextureService::refuse(const DeviceImageKeys::Key& key, const std::string& reason, bool permanent) {
    ++totals.refusals[static_cast<std::size_t>(ClassifyBindlessRefusal(reason))];
    if (permanent) {
        permanentRefusals.insert(key);
    } else {
        permanentRefusals.erase(key);
    }
    if (BindlessStatsRequested()) {
        if (auto normalized = NormalizedReason(reason); reportedReasons.insert(normalized).second) std::fprintf(stderr, "[bindless] refusal reason (first of its kind, %s, %s): %s\n", RefusalKindNames[static_cast<std::size_t>(ClassifyBindlessRefusal(reason))], permanent ? "kept" : "retried on mapping changes", normalized.c_str());
    }
    if (const auto found = rejectionIndex.find(key); found != rejectionIndex.end()) {
        if (found->second + 1u != cappedRejection) rejections[found->second] = RejectionDetail(key, reason);
        return found->second + 1u;
    }
    if (cappedRejection == 0u) {
        if (rejections.size() < MaxRejectionDetails) {
            rejections.push_back(RejectionDetail(key, reason));
        } else {
            rejections.push_back("further texture service refusal details were suppressed because the detail cap was reached");
            cappedRejection = static_cast<std::uint32_t>(rejections.size());
        }
    }
    const auto fault = cappedRejection != 0u ? cappedRejection : static_cast<std::uint32_t>(rejections.size());
    rejectionIndex.emplace(key, fault - 1u);
    ++totals.refused;
    return fault;
}

std::uint32_t TextureService::acquireImage(VkImageView view, VkImageLayout layout) {
    if (view == VK_NULL_HANDLE) return 0u;
    auto& ref = imageSlots[view];
    if (ref.references++ == 0u) {
        try {
            ref.slot = table.Heap().AllocateImage(DescriptorHeap::Array::SampledImages, view, layout);
        } catch (...) {
            imageSlots.erase(view);
            throw;
        }
    }
    return ref.slot;
}

std::uint32_t TextureService::acquireSampler(VkSampler sampler) {
    if (sampler == VK_NULL_HANDLE) return 0u;
    auto& ref = samplerSlots[sampler];
    if (ref.references++ == 0u) {
        try {
            ref.slot = table.Heap().AllocateSampler(sampler);
        } catch (...) {
            samplerSlots.erase(sampler);
            throw;
        }
    }
    return ref.slot;
}

void TextureService::discard(const BindlessResolution& resolution, Recorder& recorder) {
    if (resolution.owner != nullptr) recorder.Keep(resolution.owner);
    if (resolution.samplerOwner != nullptr) recorder.Keep(resolution.samplerOwner);
}

void TextureService::releaseResolution(const BindlessResolution& resolution, Recorder& recorder) {
    const auto serial = recorder.OpenSerial();
    if (resolution.view != VK_NULL_HANDLE) {
        auto found = imageSlots.find(resolution.view);
        Require(found != imageSlots.end() && found->second.references != 0u, "a bindless entry released an image slot it did not hold");
        if (--found->second.references == 0u) {
            table.Heap().Release(DescriptorHeap::Array::SampledImages, found->second.slot, serial);
            imageSlots.erase(found);
        }
    }
    if (resolution.sampler != VK_NULL_HANDLE) {
        auto found = samplerSlots.find(resolution.sampler);
        Require(found != samplerSlots.end() && found->second.references != 0u, "a bindless entry released a sampler slot it did not hold");
        if (--found->second.references == 0u) {
            table.Heap().Release(DescriptorHeap::Array::Samplers, found->second.slot, serial);
            samplerSlots.erase(found);
        }
    }
    if (resolution.owner != nullptr) {
        auto found = owners.find(resolution.owner.get());
        if (found != owners.end() && --found->second.references == 0u) {
            pinnedBytes -= std::min(pinnedBytes, found->second.bytes);
            owners.erase(found);
        }
        recorder.Keep(resolution.owner);
    }
    if (resolution.samplerOwner != nullptr) recorder.Keep(resolution.samplerOwner);
}

void TextureService::insertValue(const DeviceImageKeys::Key& key, const DeviceImageValue& value, Recorder& recorder) {
    if (table.Insert(key, value) != DeviceImageKeys::Insertion::Full) return;
    ++totals.fullEvictions;
    const auto wanted = std::max<std::size_t>(1u, entries.size() / 4u);
    if (evictIdle(recorder, wanted, recorder.OpenSerial(), 0u) && table.Insert(key, value) != DeviceImageKeys::Insertion::Full) return;
    const auto counts = table.Totals();
    throw std::runtime_error("AGC graphics: the bindless key table is full (" + std::to_string(counts.entries) + " entries in " + std::to_string(counts.buckets) + " buckets) and every entry is used by the batch being recorded");
}

void TextureService::place(const DeviceImageKeys::Key& key, BindlessResolution resolution, Recorder& recorder, std::uint64_t lastUsed) {
    auto value = resolution.value;
    value.slot = acquireImage(resolution.view, resolution.layout);
    try {
        value.samplerSlot = acquireSampler(resolution.sampler);
    } catch (...) {
        auto partial = resolution;
        partial.sampler = VK_NULL_HANDLE;
        partial.owner.reset();
        releaseResolution(partial, recorder);
        throw;
    }
    try {
        insertValue(key, value, recorder);
    } catch (...) {
        auto partial = resolution;
        partial.owner.reset();
        releaseResolution(partial, recorder);
        throw;
    }
    if (resolution.owner != nullptr) {
        auto& owner = owners[resolution.owner.get()];
        if (owner.references++ == 0u) {
            owner.bytes = resolution.bytes;
            pinnedBytes += resolution.bytes;
        }
    }
    const bool perUse = resolution.perUse;
    entries[key] = Entry{value, std::move(resolution), lastUsed};
    ++surfaceEpoch;
    if (perUse && perUseKeys.insert(key).second) ++perUseEpoch;
}

BindlessResolution TextureService::take(const DeviceImageKeys::Key& key) {
    auto found = entries.find(key);
    Require(found != entries.end(), "the texture service dropped an entry it does not hold");
    auto resolution = std::move(found->second.resolution);
    entries.erase(found);
    permanentRefusals.erase(key);
    ++surfaceEpoch;
    if (perUseKeys.erase(key) != 0u) ++perUseEpoch;
    return resolution;
}

void TextureService::install(const DeviceImageKeys::Key& key, BindlessResolution resolution, Recorder& recorder) {
    if (entries.contains(key)) {
        discard(resolution, recorder);
        return;
    }
    place(key, std::move(resolution), recorder, recorder.OpenSerial());
    ++totals.created;
}

void TextureService::replace(const DeviceImageKeys::Key& key, Recorder& recorder, Lock& lock, const BindlessPrepared* early) {
    lock.unlock();
    const auto started = std::chrono::steady_clock::now();
    BindlessPrepared prepared;
    bool reused = false;
    if (early != nullptr) {
        prepared = *early;
        reused = resolver->RefreshGeneration(prepared);
    }
    if (!reused) prepared = prepare(key);
    const auto prepareNanoseconds = Elapsed(started);
    BindlessResolution fresh;
    std::string refusal = prepared.refusal;
    const bool permanent = prepared.permanent;
    if (refusal.empty()) {
        try {
            fresh = settle(prepared);
        } catch (const std::runtime_error& error) {
            refusal = error.what();
            fresh = {};
        }
    }
    const auto commitNanoseconds = Elapsed(started) - prepareNanoseconds;
    lock.lock();
    totals.replacePrepareNanoseconds += prepareNanoseconds;
    totals.replaceCommitNanoseconds += commitNanoseconds;
    if (reused) ++totals.earlyUsed;
    auto found = entries.find(key);
    if (found == entries.end()) {
        discard(fresh, recorder);
        return;
    }
    if (refusal.empty() && fresh.view == found->second.resolution.view && fresh.sampler == found->second.resolution.sampler && fresh.owner == found->second.resolution.owner && fresh.value == found->second.resolution.value) {
        auto& kept = found->second.resolution;
        kept.generation = fresh.generation;
        kept.perUse = fresh.perUse;
        kept.dcc = fresh.dcc;
        if (!kept.perUse && perUseKeys.erase(key) != 0u) ++perUseEpoch;
        return;
    }
    const auto lastUsed = found->second.lastUsed;
    const bool wasPerUse = perUseKeys.contains(key);
    const auto epoch = perUseEpoch;
    const auto old = take(key);
    if (!refusal.empty()) {
        fresh = {};
        fresh.value.fault = refuse(key, refusal, permanent);
    }
    place(key, std::move(fresh), recorder, lastUsed);
    if (perUseKeys.contains(key) == wasPerUse) perUseEpoch = epoch;
    releaseResolution(old, recorder);
}

void TextureService::commitPrepared(std::vector<BindlessPrepared>& prepared, Recorder& recorder, Lock& lock) {
    for (auto& item : prepared) {
        if (entries.contains(item.key)) continue;
        lock.unlock();
        BindlessResolution resolution;
        std::string refusal = item.refusal;
        if (refusal.empty()) {
            try {
                resolution = settle(item);
            } catch (const std::runtime_error& error) {
                refusal = error.what();
                resolution = {};
            }
        }
        lock.lock();
        if (!refusal.empty()) {
            resolution = {};
            resolution.value.fault = refuse(item.key, refusal, item.permanent && refusal == item.refusal);
        }
        install(item.key, std::move(resolution), recorder);
    }
    prepared.clear();
}

void TextureService::commitTable(Tracked& tracked, Recorder& recorder, Lock& lock, std::size_t limit) {
    const auto started = std::chrono::steady_clock::now();
    while (!tracked.pending.empty() && limit != 0) {
        const auto count = std::min(limit, tracked.pending.size());
        std::vector<BindlessPrepared> batch(std::make_move_iterator(tracked.pending.end() - static_cast<std::ptrdiff_t>(count)), std::make_move_iterator(tracked.pending.end()));
        tracked.pending.resize(tracked.pending.size() - count);
        limit -= count;
        commitPrepared(batch, recorder, lock);
    }
    tracked.primerNanoseconds += Elapsed(started);
    if (!tracked.pending.empty() || tracked.state != TableState::Prepared) return;
    --preparedTables;
    tracked.state = TableState::Done;
    ++totals.primed;
    totals.primerNanoseconds += tracked.primerNanoseconds;
    changed.notify_all();
}

void TextureService::completeTable(Tracked& tracked, Recorder& recorder, Lock& lock) {
    if (tracked.state == TableState::Queued) {
        std::erase(queue, tracked.id);
        tracked.state = TableState::Reading;
        lock.unlock();
        readTable(tracked);
        lock.lock();
    }
    changed.wait(lock, [&] { return tracked.state != TableState::Reading; });
    if (tracked.state == TableState::Prepared) commitTable(tracked, recorder, lock, tracked.pending.size());
}

void TextureService::redecode(Tracked& tracked, Recorder& recorder, Lock& lock) {
    const auto& registration = tracked.registration;
    if (tracked.unmapped || tracked.pageGenerations.empty()) return;
    lock.unlock();
    memory->Collect(registration.base, static_cast<std::size_t>(tracked.span));
    const bool unchanged = memory->Unchanged(registration.base, static_cast<std::size_t>(tracked.span), tracked.oldest);
    if (unchanged) {
        lock.lock();
        return;
    }
    const auto end = registration.base + tracked.span;
    std::vector<std::pair<std::uint32_t, std::vector<std::uint32_t>>> rewritten;
    std::vector<std::uint64_t> generations = tracked.pageGenerations;
    std::uint64_t pagesChanged = 0;
    std::uint64_t recordsRead = 0;
    for (std::size_t page = 0; page < generations.size(); ++page) {
        const auto first = std::max(registration.base, tracked.firstPage + page * PageBytes);
        const auto last = std::min(end, tracked.firstPage + (page + 1u) * PageBytes);
        if (first >= last || memory->Unchanged(first, static_cast<std::size_t>(last - first), generations[page])) continue;
        ++pagesChanged;
        generations[page] = memory->Collect(first, static_cast<std::size_t>(last - first));
        const auto recordBytes = registration.dwords * 4u;
        const auto firstRecord = static_cast<std::uint32_t>(first - registration.base < recordBytes ? 0u : (first - registration.base - recordBytes) / registration.stride + 1u);
        for (auto record = firstRecord; record < registration.records; ++record) {
            const auto address = registration.base + static_cast<std::uint64_t>(record) * registration.stride;
            if (address >= last) break;
            if (address + recordBytes <= first) continue;
            std::vector<std::uint32_t> words(Abi::DescriptorWords, 0u);
            ++recordsRead;
            if (!memory->Read(address, std::span<std::uint32_t>(words).first(registration.dwords))) std::fill(words.begin(), words.end(), 0u);
            if (std::equal(words.begin(), words.end(), tracked.words.begin() + static_cast<std::ptrdiff_t>(record) * Abi::DescriptorWords)) continue;
            rewritten.emplace_back(record, std::move(words));
        }
    }
    lock.lock();
    tracked.pageGenerations = std::move(generations);
    tracked.oldest = *std::min_element(tracked.pageGenerations.begin(), tracked.pageGenerations.end());
    totals.pagesChanged += pagesChanged;
    totals.recordsRead += recordsRead;
    std::set<DeviceImageKeys::Key> wanted;
    if (!rewritten.empty()) tracked.perUseEpoch = ~std::uint64_t{0};
    for (auto& [record, words] : rewritten) {
        std::copy(words.begin(), words.end(), tracked.words.begin() + static_cast<std::ptrdiff_t>(record) * Abi::DescriptorWords);
        ++totals.recordsChanged;
        for (const auto& key : recordKeys(tracked, record)) {
            if (!entries.contains(key)) wanted.insert(key);
        }
    }
    if (wanted.empty()) return;
    std::vector<BindlessPrepared> prepared;
    lock.unlock();
    for (const auto& key : wanted) prepared.push_back(prepare(key));
    lock.lock();
    commitPrepared(prepared, recorder, lock);
}

void TextureService::reverify(Tracked& tracked, Recorder& recorder, Lock& lock) {
    tracked.evictionEpoch = evictionEpoch;
    std::set<DeviceImageKeys::Key> wanted;
    for (std::uint32_t record = 0; record < tracked.registration.records; ++record) {
        for (const auto& key : recordKeys(tracked, record)) {
            if (!entries.contains(key)) wanted.insert(key);
        }
    }
    if (wanted.empty()) return;
    std::vector<BindlessPrepared> prepared;
    lock.unlock();
    for (const auto& key : wanted) prepared.push_back(prepare(key));
    lock.lock();
    commitPrepared(prepared, recorder, lock);
}

void TextureService::revalidateServed(Tracked& tracked, Recorder& recorder, Lock& lock) {
    if (tracked.surfaceEpoch != surfaceEpoch) {
        tracked.surfaceKeys.clear();
        tracked.surfaces.clear();
        std::set<DeviceImageKeys::Key> seen;
        std::vector<std::pair<std::uint64_t, std::uint64_t>> ranges;
        for (std::uint32_t record = 0; record < tracked.registration.records; ++record) {
            for (const auto& key : recordKeys(tracked, record)) {
                if (!seen.insert(key).second) continue;
                const auto found = entries.find(key);
                if (found == entries.end() || found->second.resolution.owner == nullptr || found->second.resolution.bytes == 0u) continue;
                tracked.surfaceKeys.push_back(key);
                ranges.emplace_back(found->second.resolution.address, found->second.resolution.address + found->second.resolution.bytes);
            }
        }
        std::sort(ranges.begin(), ranges.end());
        for (const auto& range : ranges) {
            if (!tracked.surfaces.empty() && range.first <= tracked.surfaces.back().second) tracked.surfaces.back().second = std::max(tracked.surfaces.back().second, range.second);
            else tracked.surfaces.push_back(range);
        }
        tracked.surfaceEpoch = surfaceEpoch;
    }
    if (tracked.surfaceKeys.empty()) return;
    const auto surfaces = tracked.surfaces;
    const auto keys = tracked.surfaceKeys;
    lock.unlock();
    for (const auto& [begin, end] : surfaces) memory->Collect(begin, static_cast<std::size_t>(end - begin));
    lock.lock();
    std::vector<DeviceImageKeys::Key> stale;
    for (const auto& key : keys) {
        const auto found = entries.find(key);
        if (found == entries.end() || found->second.resolution.owner == nullptr) continue;
        const auto& resolution = found->second.resolution;
        ++totals.servedChecked;
        if (resolution.bytes == 0u || (memory->Unchanged(resolution.address, static_cast<std::size_t>(resolution.bytes), resolution.generation) && resolver->Current(resolution))) continue;
        stale.push_back(key);
    }
    for (const auto& key : stale) {
        if (!entries.contains(key)) continue;
        ++totals.servedStale;
        replace(key, recorder, lock);
    }
}

std::vector<DeviceImageMiss> TextureService::unresolved(std::span<const DeviceImageMiss> misses) const {
    std::vector<DeviceImageMiss> fresh;
    fresh.reserve(misses.size());
    for (const auto& miss : misses) {
        if (!entries.contains(miss.key)) fresh.push_back(miss);
    }
    return fresh;
}

void TextureService::resolveMisses(Recorder& recorder, Lock& lock) {
    changed.wait(lock, [&] { return !takingMisses; });
    lock.unlock();
    const auto misses = table.TakeMisses();
    lock.lock();
    const auto fresh = unresolved(misses);
    lock.unlock();
    std::vector<BindlessPrepared> prepared;
    for (const auto& miss : fresh) prepared.push_back(prepare(miss.key));
    lock.lock();
    totals.misses += misses.size();
    prepared.insert(prepared.end(), std::make_move_iterator(missPending.begin()), std::make_move_iterator(missPending.end()));
    missPending.clear();
    totals.missesResolved += prepared.size();
    commitPrepared(prepared, recorder, lock);
}

void TextureService::refreshPerUse(std::span<const std::uint64_t> served, Recorder& recorder, Lock& lock) {
    if (perUseKeys.empty()) return;
    std::set<DeviceImageKeys::Key> keys;
    for (const auto id : served) {
        auto found = tables.find(id);
        if (found == tables.end()) continue;
        auto& tracked = found->second;
        if (tracked.perUseEpoch != perUseEpoch) {
            tracked.perUse.clear();
            for (std::uint32_t record = 0; record < tracked.registration.records; ++record) {
                for (const auto& key : recordKeys(tracked, record)) {
                    if (perUseKeys.contains(key)) tracked.perUse.push_back(key);
                }
            }
            tracked.perUseEpoch = perUseEpoch;
        }
        keys.insert(tracked.perUse.begin(), tracked.perUse.end());
    }
    for (const auto& key : keys) {
        const auto found = entries.find(key);
        if (found == entries.end() || !found->second.resolution.perUse) continue;
        ++totals.refreshed;
        if (found->second.resolution.dcc) ++totals.dccRefreshed;
        replace(key, recorder, lock);
    }
}

std::uint64_t TextureService::currentBudget() {
    if (options.budgetSource) {
        if (const auto live = options.budgetSource(); live != 0) budgetBytes = live;
    }
    return budgetBytes;
}

bool TextureService::evictIdle(Recorder& recorder, std::size_t wanted, std::uint64_t idleBefore, std::uint64_t targetBytes) {
    std::set<DeviceImageKeys::Key> protectedKeys;
    for (const auto& [id, tracked] : tables) {
        if (tracked.lastUsed < idleBefore) continue;
        for (std::uint32_t record = 0; record < tracked.registration.records; ++record) {
            for (const auto& key : recordKeys(tracked, record)) protectedKeys.insert(key);
        }
    }
    std::vector<std::pair<std::uint64_t, DeviceImageKeys::Key>> idle;
    for (const auto& [key, entry] : entries) {
        if (entry.lastUsed >= idleBefore || protectedKeys.contains(key) || retiring.contains(key)) continue;
        idle.emplace_back(entry.lastUsed, key);
    }
    std::sort(idle.begin(), idle.end());
    std::uint64_t projected = pinnedBytes;
    std::map<const void*, std::uint32_t> dropping;
    std::size_t taken = 0;
    for (const auto& [used, key] : idle) {
        if (targetBytes != 0 ? projected <= targetBytes : taken >= wanted) break;
        retiring.insert(key);
        ++taken;
        ++totals.evicted;
        const auto& owner = entries.at(key).resolution.owner;
        if (owner == nullptr) continue;
        const auto found = owners.find(owner.get());
        if (found != owners.end() && ++dropping[owner.get()] == found->second.references) projected -= std::min(projected, found->second.bytes);
    }
    if (taken == 0) return false;
    retireNow(recorder);
    return true;
}

void TextureService::retireNow(Recorder& recorder) {
    if (retiring.empty()) return;
    const auto dropped = table.Retire([&](const DeviceImageKeys::Key& key, const DeviceImageValue&) { return retiring.contains(key); });
    static_cast<void>(dropped);
    for (const auto& key : retiring) {
        if (!entries.contains(key)) continue;
        releaseResolution(take(key), recorder);
        ++totals.retired;
    }
    retiring.clear();
    ++evictionEpoch;
}

void TextureService::checkMapping(Recorder& recorder) {
    const auto serial = memory->MappingSerial();
    if (serial == mappingSerial) return;
    mappingSerial = serial;
    for (auto it = tables.begin(); it != tables.end();) {
        auto& tracked = it->second;
        if (tracked.state == TableState::Done && !memory->Mapped(tracked.registration.base, static_cast<std::size_t>(tracked.span))) {
            ids.erase(tracked.registration);
            it = tables.erase(it);
            ++totals.unmapped;
            continue;
        }
        ++it;
    }
    for (const auto& [key, entry] : entries) {
        if (entry.value.fault != 0u) {
            if (permanentRefusals.contains(key)) ++totals.keptRefusals;
            else retiring.insert(key);
            continue;
        }
        if (entry.resolution.bytes == 0 || memory->Mapped(entry.resolution.address, static_cast<std::size_t>(entry.resolution.bytes))) continue;
        retiring.insert(key);
        ++totals.unmapped;
    }
    retireNow(recorder);
}

void TextureService::Serve(std::span<const std::uint64_t> served, Recorder& recorder) {
    Require(GuestMemory::GpuMutex().HeldByThisThread(), "the texture service publishes only under the GPU lock");
    const auto started = std::chrono::steady_clock::now();
    {
        Lock lock(mutex);
        const auto serial = recorder.OpenSerial();
        for (const auto id : served) {
            auto found = tables.find(id);
            Require(found != tables.end(), "the texture service was asked to serve a table it does not hold (unregistered or unmapped)");
            auto& tracked = found->second;
            tracked.lastUsed = serial;
            if (tracked.state != TableState::Done) {
                completeTable(tracked, recorder, lock);
            } else {
                redecode(tracked, recorder, lock);
                if (tracked.evictionEpoch != evictionEpoch) reverify(tracked, recorder, lock);
            }
            revalidateServed(tracked, recorder, lock);
        }
        resolveMisses(recorder, lock);
        refreshPerUse(served, recorder, lock);
        retireNow(recorder);
        ++totals.serves;
    }
    table.Publish(recorder);
    std::lock_guard lock(mutex);
    totals.serveNanoseconds += Elapsed(started);
}

std::uint64_t TextureService::PresentSerial(Recorder& recorder) {
    const auto counts = table.Totals();
    std::lock_guard lock(mutex);
    if (counts.rings == lastRings) return 0u;
    lastRings = counts.rings;
    recorder.Submit();
    return recorder.Submissions();
}

void TextureService::PrepareRevalidation() {
    const auto started = std::chrono::steady_clock::now();
    struct Candidate {
        DeviceImageKeys::Key key;
        std::uint64_t address = 0;
        std::uint64_t bytes = 0;
        std::uint64_t generation = 0;
    };
    std::vector<Candidate> candidates;
    const auto mapping = memory->MappingSerial();
    {
        Lock lock(mutex);
        early.clear();
        candidates.reserve(entries.size());
        for (const auto& [key, entry] : entries) {
            if (entry.resolution.owner == nullptr || entry.resolution.bytes == 0) continue;
            candidates.push_back({key, entry.resolution.address, entry.resolution.bytes, entry.resolution.generation});
        }
    }
    std::erase_if(candidates, [&](const Candidate& candidate) { return memory->Unchanged(candidate.address, static_cast<std::size_t>(candidate.bytes), candidate.generation); });
    std::vector<BindlessPrepared> prepared;
    prepared.reserve(candidates.size());
    for (const auto& candidate : candidates) {
        auto item = prepare(candidate.key);
        if (item.refusal.empty()) prepared.push_back(std::move(item));
    }
    Lock lock(mutex);
    early.clear();
    for (auto& item : prepared) {
        const auto key = item.key;
        early.insert_or_assign(key, std::move(item));
    }
    earlyMapping = mapping;
    totals.earlyPrepared += early.size();
    totals.earlyNanoseconds += Elapsed(started);
}

void TextureService::Revalidate(Recorder& recorder) {
    const auto started = std::chrono::steady_clock::now();
    Lock lock(mutex);
    auto prepared = std::exchange(early, {});
    if (!prepared.empty() && memory->MappingSerial() != earlyMapping) {
        totals.earlyDropped += prepared.size();
        prepared.clear();
    }
    checkMapping(recorder);
    std::vector<std::pair<std::uint64_t, std::uint64_t>> ranges;
    for (const auto& [key, entry] : entries) {
        if (entry.resolution.bytes != 0 && entry.resolution.owner != nullptr) ranges.emplace_back(entry.resolution.address, entry.resolution.address + entry.resolution.bytes);
    }
    std::sort(ranges.begin(), ranges.end());
    std::vector<std::pair<std::uint64_t, std::uint64_t>> merged;
    for (const auto& range : ranges) {
        if (!merged.empty() && range.first <= merged.back().second) merged.back().second = std::max(merged.back().second, range.second);
        else merged.push_back(range);
    }
    lock.unlock();
    const auto collectStarted = std::chrono::steady_clock::now();
    for (const auto& [begin, end] : merged) memory->Collect(begin, static_cast<std::size_t>(end - begin));
    const auto collectNanoseconds = Elapsed(collectStarted);
    lock.lock();
    totals.revalidateCollectNanoseconds += collectNanoseconds;
    const auto checkStarted = std::chrono::steady_clock::now();
    std::vector<DeviceImageKeys::Key> stale;
    for (const auto& [key, entry] : entries) {
        if (entry.resolution.owner == nullptr) continue;
        ++totals.revalidated;
        const bool unchanged = entry.resolution.bytes == 0 || memory->Unchanged(entry.resolution.address, static_cast<std::size_t>(entry.resolution.bytes), entry.resolution.generation);
        if (unchanged && resolver->Current(entry.resolution)) continue;
        stale.push_back(key);
    }
    totals.revalidateCheckNanoseconds += Elapsed(checkStarted);
    for (const auto& key : stale) {
        if (!entries.contains(key)) continue;
        ++totals.stale;
        const auto found = prepared.find(key);
        if (found == prepared.end()) {
            replace(key, recorder, lock);
            continue;
        }
        const auto item = std::move(found->second);
        prepared.erase(found);
        replace(key, recorder, lock, &item);
    }
    totals.earlyDropped += prepared.size();
    const auto budget = currentBudget();
    if (budget != 0 && pinnedBytes > budget) {
        const auto open = recorder.OpenSerial();
        const auto idleBefore = open > options.idleSubmits ? open - options.idleSubmits : 0u;
        if (idleBefore != 0) evictIdle(recorder, 0u, idleBefore, budget / 4u * 3u);
    }
    retireNow(recorder);
    totals.revalidateNanoseconds += Elapsed(started);
}

void TextureService::AfterPresentWait(Recorder& recorder, std::uint64_t waitedNanoseconds) {
    {
        std::lock_guard lock(mutex);
        ++frame;
        if (waitedNanoseconds != 0) {
            ++totals.presentWaits;
            totals.presentWaitNanoseconds += waitedNanoseconds;
        }
    }
    Revalidate(recorder);
    table.ForgetReportedMisses();
    const auto started = std::chrono::steady_clock::now();
    Publish(recorder);
    std::lock_guard lock(mutex);
    totals.publishNanoseconds += Elapsed(started);
}

std::optional<DeviceImageValue> TextureService::Find(const DeviceImageKeys::Key& key) const {
    std::lock_guard lock(mutex);
    const auto found = entries.find(key);
    if (found == entries.end()) return std::nullopt;
    return found->second.value;
}

std::shared_ptr<void> TextureService::Owner(const DeviceImageKeys::Key& key) const {
    std::lock_guard lock(mutex);
    const auto found = entries.find(key);
    if (found == entries.end()) return nullptr;
    return found->second.resolution.owner;
}

std::optional<std::string> TextureService::Rejection(std::uint32_t fault) const {
    std::lock_guard lock(mutex);
    if (fault == 0u || fault > rejections.size()) return std::nullopt;
    return rejections[fault - 1u];
}

std::size_t TextureService::queueDepth() const {
    std::size_t depth = queue.size() + missPending.size();
    for (const auto& [id, tracked] : tables) depth += tracked.pending.size() + (tracked.state == TableState::Reading ? 1u : 0u);
    return depth;
}

void TextureService::hostBytes(Counts& counts) const {
    const auto vectorBytes = [](const auto& values) { return static_cast<std::uint64_t>(values.capacity() * sizeof(values[0])); };
    counts.tables = tables.size();
    counts.tableHostBytes = 0;
    for (const auto& [id, tracked] : tables) {
        counts.tableHostBytes += sizeof(tracked) + vectorBytes(tracked.words) + vectorBytes(tracked.pageGenerations) + vectorBytes(tracked.pending) + vectorBytes(tracked.surfaceKeys);
        counts.tableHostBytes += vectorBytes(tracked.surfaces) + vectorBytes(tracked.perUse) + 2u * vectorBytes(tracked.registration.uses);
    }
    counts.entryHostBytes = entries.size() * (sizeof(DeviceImageKeys::Key) + sizeof(Entry) + 2u * sizeof(void*)) + entries.bucket_count() * sizeof(void*);
    counts.rejections = rejections.size();
    counts.rejectionHostBytes = (rejectionIndex.size() + permanentRefusals.size()) * (sizeof(DeviceImageKeys::Key) + 4u * sizeof(void*));
    for (const auto& rejection : rejections) counts.rejectionHostBytes += sizeof(rejection) + rejection.capacity();
}

TextureService::Counts TextureService::Totals() const {
    std::lock_guard lock(mutex);
    auto counts = totals;
    counts.queued = queueDepth();
    counts.entries = entries.size();
    counts.imageSlots = imageSlots.size();
    counts.samplerSlots = samplerSlots.size();
    counts.pinnedBytes = pinnedBytes;
    counts.budgetBytes = budgetBytes;
    hostBytes(counts);
    return counts;
}

std::string TextureService::TakeFrameLine(std::uint64_t oldPathNanoseconds) {
    const auto ring = table.Totals();
    const auto now = Totals();
    std::lock_guard lock(mutex);
    const auto dropped = ring.dropped - lastDropped;
    lastDropped = ring.dropped;
    const auto records = ring.misses - lastRecords;
    lastRecords = ring.misses;
    const auto clears = ring.seenClears - lastSeenClears;
    lastSeenClears = ring.seenClears;
    const auto oldPath = oldPathNanoseconds - std::min(oldPathNanoseconds, reportedOldPath);
    reportedOldPath = oldPathNanoseconds;
    const auto delta = [](std::uint64_t current, std::uint64_t previous) { return static_cast<unsigned long long>(current - previous); };
    std::string refusals;
    for (std::size_t kind = 0; kind < RefusalKindNames.size(); ++kind) {
        refusals += kind == 0u ? "" : ", ";
        refusals += RefusalKindNames[kind];
        refusals += ' ';
        refusals += std::to_string(now.refusals[kind] - reported.refusals[kind]);
    }
    char text[2048];
    std::snprintf(text, sizeof(text), "[bindless] frame %llu: misses %llu (%llu dropped), created %llu, refused %llu, primed %llu tables (%llu records, %llu new keys, primer %.1f ms), re-decoded %llu pages (%llu records changed), served-checked %llu (%llu stale), queue %llu, revalidated %llu (%llu stale, %llu per-use, %llu dcc), retired %llu (%llu unmapped, %llu evicted, %llu full), serve %.1f ms over %llu calls, present wait %.1f ms, entries %llu, slots %llu+%llu, pinned %.1f MiB of a %llu MiB budget, old-path resolve %.1f ms, refusals (%s), kept refusals %llu, host: tables %llu (%.1f MiB), entries %.1f MiB, rejections %llu (%.1f MiB), miss ring: %llu records, %llu set clears; revalidate %.1f ms under the GPU lock (collect %.1f, check %.1f, prepare %.1f, commit %.1f), publish %.1f ms, early prepare %.1f ms off the lock (%llu prepared, %llu used, %llu dropped)",
        static_cast<unsigned long long>(frame), delta(now.misses, reported.misses), static_cast<unsigned long long>(dropped), delta(now.created, reported.created), delta(now.refused, reported.refused), delta(now.primed, reported.primed), delta(now.primedRecords, reported.primedRecords), delta(now.primedKeys, reported.primedKeys), Milliseconds(now.primerNanoseconds - reported.primerNanoseconds), delta(now.pagesChanged, reported.pagesChanged), delta(now.recordsChanged, reported.recordsChanged), delta(now.servedChecked, reported.servedChecked), delta(now.servedStale, reported.servedStale), static_cast<unsigned long long>(now.queued), delta(now.revalidated, reported.revalidated), delta(now.stale, reported.stale), delta(now.refreshed, reported.refreshed), delta(now.dccRefreshed, reported.dccRefreshed), delta(now.retired, reported.retired), delta(now.unmapped, reported.unmapped), delta(now.evicted, reported.evicted), delta(now.fullEvictions, reported.fullEvictions), Milliseconds(now.serveNanoseconds - reported.serveNanoseconds), delta(now.serves, reported.serves), Milliseconds(now.presentWaitNanoseconds - reported.presentWaitNanoseconds), static_cast<unsigned long long>(now.entries), static_cast<unsigned long long>(now.imageSlots), static_cast<unsigned long long>(now.samplerSlots), static_cast<double>(now.pinnedBytes) / static_cast<double>(1u << 20u), static_cast<unsigned long long>(now.budgetBytes >> 20u), Milliseconds(oldPath), refusals.c_str(), delta(now.keptRefusals, reported.keptRefusals), static_cast<unsigned long long>(now.tables), static_cast<double>(now.tableHostBytes) / static_cast<double>(1u << 20u), static_cast<double>(now.entryHostBytes) / static_cast<double>(1u << 20u), static_cast<unsigned long long>(now.rejections), static_cast<double>(now.rejectionHostBytes) / static_cast<double>(1u << 20u), static_cast<unsigned long long>(records), static_cast<unsigned long long>(clears), Milliseconds(now.revalidateNanoseconds - reported.revalidateNanoseconds), Milliseconds(now.revalidateCollectNanoseconds - reported.revalidateCollectNanoseconds), Milliseconds(now.revalidateCheckNanoseconds - reported.revalidateCheckNanoseconds), Milliseconds(now.replacePrepareNanoseconds - reported.replacePrepareNanoseconds), Milliseconds(now.replaceCommitNanoseconds - reported.replaceCommitNanoseconds), Milliseconds(now.publishNanoseconds - reported.publishNanoseconds), Milliseconds(now.earlyNanoseconds - reported.earlyNanoseconds), delta(now.earlyPrepared, reported.earlyPrepared), delta(now.earlyUsed, reported.earlyUsed), delta(now.earlyDropped, reported.earlyDropped));
    reported = now;
    return text;
}

}
