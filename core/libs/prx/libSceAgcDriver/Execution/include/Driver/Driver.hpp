#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_DRIVER_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_DRIVER_HPP

#include "prx/libSceAgcDriver/Execution/include/Driver/Queues/Submission.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Queues/SubmissionRing.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/DeviceAccess.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Packets/PacketHistory.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Dispatch/DispatchCache.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawCache.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Dispatch/DispatchTiming.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawTiming.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Memory/BufferCopy.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Dispatch/IndirectDispatch.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Memory/WriteEvidence.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Packets/PacketTiming.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Synchronization/WaitMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Pm4.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderInputState.hpp"
#include "prx/libSceAgcDriver/Execution/include/Mutex.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <exception>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace AgcDriver {
class PerformanceTimer;
}

namespace AgcDriver::DriverDetail {

struct DrawTail;

class Driver {
public:
    static Driver& Get();
    static std::uint64_t GpuMapImportThreads();
    ~Driver();
    void Shutdown();
    void WaitIdle();
    std::uint64_t DispatchesAdoptedAhead() const;
    std::uint64_t DispatchesRejectedAhead() const;
    void SetAheadAdoptHook(void (*hook)());
    void CheckFailure();
    void ReportFailure(std::exception_ptr error);
    void Submit(const Packet* packet, std::uint32_t queue, bool cpuFlip = false);
    void SubmitFlip(const FlipInfo& info);
    void SuspendPoint();
    std::uint32_t ReserveEopFlip(const FlipInfo& info);
    std::uint32_t ReserveEopFlipAt(const FlipInfo& info, std::uint32_t token);
    std::vector<std::uint32_t> PendingEopFlipTokens();
    std::pair<std::uint64_t, std::uint64_t> SerialProgress();
    std::uint64_t FlipSubmissions() const;
    std::vector<AwaitedWait> AwaitedWaits();
    bool WaitIdleFor(std::chrono::milliseconds timeout);
    void RegisterVideoOutput(std::uint32_t handle, const std::shared_ptr<IVideoOutput>& output);
    void UnregisterVideoOutput(std::uint32_t handle, const std::shared_ptr<IVideoOutput>& output);
    void AttachWindow(const PresentationWindow& window);
    void Present(const PresentationWindow& window, const DisplayBuffer* buffer, bool opaque, void (*gpuReady)(void*), void* context);
    void ReleaseWindow(void* window);
    void RegisterShader(const Shader* shader);
    void ResolveShaderAbi(const Shader* shader, std::span<const ShaderRegister> context, std::span<const ShaderRegister> primitive);
    void ResolveGraphicsStagesAbi(std::span<const Shader* const> stages, std::span<const ShaderRegister> context, std::span<const ShaderRegister> primitive);
    void ResolveGraphicsAbi(const Shader* vertex, const Shader* pixel, std::uint32_t primitiveType);
    void SettleShaderPreparations(std::span<const Shader* const> stages);
    [[nodiscard]] std::size_t PreparedEntries(const Shader* shader);
    struct DeferredAbiUseCounts {
        std::uint64_t firstUses;
        std::uint64_t found;
        std::uint64_t own;
        std::uint64_t entries;
        std::uint64_t unmatchedEntries;
    };
    [[nodiscard]] static DeferredAbiUseCounts DeferredAbiUses();
    [[nodiscard]] std::vector<std::array<std::uint64_t, 2>> PreparedEntryHashes(const Shader* shader);
    [[nodiscard]] std::array<std::uint64_t, 4> PrebuiltLibraries();
    static void FoldDrawOffsets(const ShaderRecompiler::RecompileResult& result, const DrawProgram& program, Pm4::DrawParameters& parameters);
    static std::optional<Graphics::IndirectDrawPath> ClassifyIndirectDraw(const ShaderRecompiler::RecompileResult& result, const Graphics::State& graphics, const DrawProgram& frontProgram, const std::shared_ptr<VulkanDevice>& localDevice, Pm4::DrawParameters& drawParameters, bool traceIndirect);
    static void tagVariantRuns(DispatchVariant& variant, std::span<const ShaderRecompiler::MemoryRegion> regions, const ShaderRecompiler::SrtUserDataUse& use, bool eligible, std::uint32_t hiddenWords, std::span<const std::shared_ptr<const ShaderRecompiler::SrtGuestWords>> supplied = {});
    static std::string dumpRequest(std::uint64_t address, const ShaderRecompiler::RecompileRequest& request, std::string_view suffix = {});
    DrawEntryCounters DrawCacheCounters();
    EntryCounters DispatchCacheCounters();
    struct DispatchCacheFootprint {
        std::size_t entries = 0;
        std::uint64_t variantBytes = 0;
    };
    DispatchCacheFootprint DispatchCacheSize();
    static std::uint64_t DispatchCacheBytes();
    static ShaderMemory::PendingWrite QueryPendingSpan(std::uint64_t address, std::size_t bytes, std::span<std::byte> known);
    std::shared_ptr<const ShaderRegistry> RegisteredShaders();

private:
    friend class SampledReadScope;
    friend struct PendingView;
    friend struct PacketTimer;
    friend struct DriverTestAccess;
    void stop();
    Driver();
    void rethrowFailure() const;
    void checkStopping() const;
    std::uint32_t reserveEopFlip(const FlipInfo& info, std::uint32_t token);
    static bool& onWorkerThread();
    static void copyCommands(Submission& submission, const std::uint32_t* guest, std::size_t words);
    static bool copySegment(Submission& submission, const std::uint32_t* guest, std::size_t words, std::size_t& budget);
    static void readRegisterLists(Submission& submission);
    void waitForFlipRoom(const Submission& submission);
    void holdFlipBehindWorker(const Submission& submission);
    bool awaitsTitle(std::uint64_t awaited) const;
    void noteAwaitingTitle(std::uint32_t queue, std::uint64_t awaited);
    void releaseFlipHold();
    void reserveOutputs(Submission& submission);
    void executeRewindTail(const Submission& stalled);
    void enqueue(Submission submission);
    void noteHeldAtSubmit(Submission& submission, std::size_t cursor);
    static void forgetUnfinishedWrites(QueueWorker& worker, const Submission& submission);
    static void forgetUnfinishedWrites(QueueWorker& worker, std::span<const std::uint64_t> labelWrites);
    static bool& completionDeferred();
    void deferCompletion(const Submission& submission);
    void submitOpenWork();
    void wakeQueueZeroForCompletions();
    void completeSubmission(std::uint64_t serial, std::uint64_t received, std::span<const std::uint64_t> labelWrites);
    static bool waitFree(const Submission& submission);
    bool queue0Before(std::uint64_t received) const;
    bool orderReleased(std::uint32_t queue, std::uint64_t received) const;
    void noteWaitBlocked(std::uint32_t queue, std::uint64_t awaited, bool blocked);
    void reportPresents(double waitedMs, std::size_t inFlight);
    static bool stampValidate();
    static bool dataHits();
    static bool verifyDataHits();
    static bool pipelineDispatch();
    static std::size_t dispatchVariants();
    static bool insertCompare();
    static std::size_t dispatchCacheEntries();
    static std::uint64_t variantBytes(const DispatchVariant& variant);
    void accountVariant(const DispatchVariant& variant, bool added);
    void eraseDispatchEntry(std::unordered_map<std::uint64_t, std::shared_ptr<DispatchEntry>>::iterator it);
    void classifyDiffering(std::uint64_t program, std::uint64_t key, const DispatchVariant& old, const DispatchVariant& fresh, const ShaderRecompiler::ResourceCapture* capture, EntryCounters& counters);
    void reportDispatchCache(EntryCounters& counters);
    void lookupDispatch(std::uint64_t address, const Submission& submission, std::uint64_t key, bool noDispatchCache, bool traceCache, bool profile, std::span<const ShaderRecompiler::MemoryRegion> memory, DispatchPhaseTiming& phaseTiming, std::array<double, DriverPhaseCount>& phaseMs, std::shared_ptr<const ShaderRecompiler::RecompileResult>& compiledResult, std::shared_ptr<DispatchVariant>& keepVariant, std::vector<ShaderRecompiler::MemoryRegion>& captured, std::vector<std::uint32_t>& liveWords, bool& dataHit, bool& cached, bool& validated, std::shared_ptr<DispatchEntry>& missedEntry, bool& missedDiffering);
    void insertDispatch(std::uint64_t address, std::uint64_t key, bool noDispatchCache, bool profile, const std::shared_ptr<const ShaderSnapshot>& registeredShader, std::uint64_t forgetAtCapture, std::span<const ShaderRecompiler::MemoryRegion> memory, const std::shared_ptr<ShaderMemory>& shaderMemory, const std::vector<ShaderRecompiler::MemoryRegion>& captured, const std::shared_ptr<const ShaderRecompiler::ResourceCapture>& capture, const std::shared_ptr<const ShaderRecompiler::RecompileResult>& compiledResult, const std::shared_ptr<DispatchEntry>& missedEntry, bool missedDiffering, std::shared_ptr<DispatchVariant>& attachVariant, DispatchPhaseTiming& phaseTiming);
    void dispatch(QueueState& queue, std::span<const std::uint32_t> packet, const Submission& submission, std::uint64_t indirectArguments = 0);
    void verifyDataHit(const ShaderSnapshot& snapshot, std::size_t codeOffset, ShaderRecompiler::RecompileRequest request, std::span<const ShaderRecompiler::MemoryRegion> memory, std::uint64_t address, const DispatchVariant& variant, std::span<const std::uint32_t> liveWords, const ShaderRecompiler::RecompileResult& patched);
    static bool drawEntries();
    static bool drawDataHits();
    static bool verifyDrawDataHits();
    static bool drawStageDataHits();
    static bool verifyDrawEntries();
    static bool registerKeyEnabled();
    static bool verifyDrawRecipe();
    static std::size_t drawCacheEntries();
    void accountDrawVariant(const DispatchVariant& variant, bool added);
    void insertDrawEntry(std::uint64_t key, std::vector<std::shared_ptr<DispatchVariant>>& fresh, std::shared_ptr<const DrawDecode> decode, std::uint64_t shape = 0, bool replaceShape = false);
    bool admitDrawKey(std::uint64_t key, std::uint64_t shape);
    void eraseDrawEntry(std::uint64_t key, const std::shared_ptr<DrawEntry>& expected);
    std::shared_ptr<const DrawRecipe> findDrawRecipe(std::uint64_t key, const std::vector<std::shared_ptr<DispatchVariant>>& stages);
    void attachDrawRecipe(std::uint64_t key, const std::vector<std::shared_ptr<DispatchVariant>>& stages, std::shared_ptr<const DrawRecipe> recipe);
    static bool traceDrawCache();
    static void traceInsert(std::unordered_set<std::uint64_t>& set, std::uint64_t key);
    void maybeReportDrawCache(bool profile);
    void reportDrawCache(DrawEntryCounters& counters, bool profile);
    std::string profileDrawCacheLines(const DrawEntryCounters& counters);
    static std::uint64_t drawRegisterKey(const QueueState& queue, const ShaderRegistry& registry, std::uint64_t deviceSerial, std::uint64_t* shape = nullptr, std::array<std::uint64_t, 5>* components = nullptr, const DrawRelocation* relocation = nullptr, std::uint64_t* alias = nullptr);
    void traceDrawKeyLookup(const std::array<std::uint64_t, 5>& components, bool cacheMiss);
    static std::uint64_t drawIdentityKey(const QueueState& queue, const ShaderRegistry& registry);
    static bool drawRelocationEnabled();
    static std::uint64_t relocatedContentKey(const RelocatableUserWord& word, std::uint32_t value, bool& readable);
    static void applyShaderDataPatches(DispatchVariant& variant);
    void learnPendingRelocation();
    void relocateDrawEntry(const QueueState& queue, std::uint64_t alias, std::shared_ptr<DrawEntry>& entry, std::shared_ptr<const DrawDecode>& decode, std::vector<DrawUserPatch>& patches);
    void reportDrawRelocation();
    static bool sameVertexInfo(const ShaderRecompiler::ShaderVertexStageInfo& a, const ShaderRecompiler::ShaderVertexStageInfo& b);
    static bool sameDecode(const DrawDecode& a, const DrawDecode& b);
    std::shared_ptr<DrawDecode> decodeDraw(const QueueState& queue, const Submission& submission, const Graphics::Context& context);
    void resolveDrawDecode(const QueueState& queue, const Submission& submission, const Graphics::Context& context, std::shared_ptr<const DrawDecode>& decode, bool registerKey, std::uint64_t drawKey, bool profile);
    void lookupDraw(const Submission& submission, const std::shared_ptr<VulkanDevice>& localDevice, const Graphics::State& graphics, const ShaderRecompiler::ShaderPixelStageInfo& pixel, const std::vector<DrawProgram>& programs, const std::vector<ShaderRecompiler::ProgramRole>& roles, const std::vector<std::optional<ShaderRecompiler::ShaderVertexStageInfo>>& vertexInfos, bool useDrawEntries, bool registerKey, bool profile, std::uint64_t& drawKey, std::shared_ptr<DrawEntry>& entry, std::vector<std::shared_ptr<DispatchVariant>>& matched, std::vector<std::vector<ShaderRecompiler::MemoryRegion>>& matchedRegions, bool& drawHit, bool& verifyHit, DrawPhaseTiming& phaseTiming, std::array<double, DrawDriverPhaseCount>& phaseMs, const std::shared_ptr<DrawEntry>& dataEntry, DrawDataCandidates& dataCandidates, std::vector<StageDataHit>& stageData);
    std::shared_ptr<const ShaderRecompiler::RecompileResult> materializeDrawStage(std::size_t i, std::uint32_t pushOffset, const QueueState& queue, const Submission& submission, const std::vector<DrawProgram>& programs, const Graphics::State& graphics, const ShaderRecompiler::ShaderPixelStageInfo& pixel, const std::vector<std::optional<ShaderRecompiler::ShaderVertexStageInfo>>& vertexInfos, std::vector<ShaderRecompiler::MemoryRegion>& memory, const std::vector<ShaderRecompiler::LinkedProgram>& linked, const Pm4::DrawParameters& drawParameters, const std::shared_ptr<VulkanDevice>& localDevice, ShaderMemory& shaderMemory, std::vector<StageCapture>& stageCaptures, std::vector<bool>& recompiled, bool drawHit, const std::vector<std::shared_ptr<DispatchVariant>>& matched, const std::vector<std::vector<ShaderRecompiler::MemoryRegion>>& matchedRegions, bool profile, std::uint64_t dumpTarget, std::uint64_t dumpSlot1, std::uint64_t& captures, DrawPhaseTiming& phaseTiming, std::array<double, DrawDriverPhaseCount>& phaseMs, std::string& rejected, const std::optional<ShaderRecompiler::MeshBarycentricLayout>& meshBarycentrics);
    static void recordStageDataPositions(DispatchVariant& variant, const ShaderRecompiler::ResourceCapture& capture, const std::vector<Graphics::DecodeRead>& decodeReads);
    void cacheDrawStages(bool useDrawEntries, bool drawHit, const Pm4::DrawParameters& drawParameters, const std::optional<Graphics::IndirectDrawPath>& indirectCpu, const std::vector<DrawProgram>& programs, const std::vector<StageCapture>& stageCaptures, const std::vector<std::optional<ShaderRecompiler::ShaderVertexStageInfo>>& vertexInfos, const std::vector<std::vector<Graphics::DecodeRead>>& decodeReads, bool verifyHit, const std::vector<std::shared_ptr<DispatchVariant>>& matched, const std::vector<StageDataHit>& stageData, std::vector<std::shared_ptr<DispatchVariant>>& fresh, std::uint64_t drawKey, bool registerKey, const std::shared_ptr<const DrawDecode>& decode, DrawPhaseTiming& phaseTiming, const std::vector<bool>& reused, const std::shared_ptr<DrawEntry>& entry, std::uint64_t shapeKey, bool shapeNeedsEntry = false);
    static bool drawPrecheck();
    std::optional<DrawVerdict> precheckDraw(const QueueState& queue, const Submission& submission, std::span<const std::uint32_t> packet, const Pm4::DrawParameters& drawParameters, std::string& rejected, bool& traceIndirect);
    static std::uint32_t drawUserWord(const DrawProgram& program, std::int32_t sgpr);
    DrawVerdict draw(QueueState& queue, std::span<const std::uint32_t> packet, const Submission& submission, std::string& rejected);
    DrawVerdict finishDraw(DrawTail& tail, PerformanceTimer& timing);
    void addDriverPhases(DispatchClass which, const std::array<double, DriverPhaseCount>& ms, bool hit, bool validated);
    static PendingDispatchPhases& pendingDispatchPhases();
    static std::chrono::steady_clock::time_point& packetStartedAt();
    static PendingDrawPhases& pendingDrawPhases();
    void addDrawPhases(const std::array<double, DrawDriverPhaseCount>& ms, bool drawn, std::uint64_t captures);
    void noteLabelStore(std::uint64_t address, std::span<const std::byte> bytes, std::uint64_t stamp, std::uint32_t queue);
    bool storedSince(std::span<const std::uint32_t> packet, std::uint64_t address, std::size_t bytes, std::uint64_t received, std::uint32_t* writer = nullptr);
    static void traceLabel(std::span<const std::uint32_t> packet, std::uint32_t queue);
    static std::array<WriteRecord, 16384>& writeHistory();
    static std::size_t& writeCursor();
    static void dumpPackets(std::span<const std::uint32_t> commands, const std::uint32_t* guest = nullptr);
    static void validate(const Submission& submission, const std::uint32_t* guest = nullptr);
    static void reportSkip(const char* kind, const std::string& what);
    static bool matchesFillKernel(std::span<const std::uint32_t> code, const std::vector<std::uint32_t>& userData, const ShaderRecompiler::ShaderComputeStageInfo& compute);
    static bool fillClearEnabled();
    static bool fillClearExactOnly();
    static void fillClearCount(const Graphics::StorageTexture::FillCoverage& coverage, std::size_t bytes, std::span<const std::uint32_t, 4> pattern, std::size_t discarded, bool cleared, const char* refusal);
    void storeBufferFill(std::uint32_t queueId, std::uint64_t base, std::size_t bytes, const std::array<std::uint32_t, 4>& pattern, const std::shared_ptr<VulkanDevice>& localDevice, bool memoryValueFill, bool noteMetadata);
    bool fillBuffer(QueueState& queue, std::uint32_t queueId, std::span<const std::uint32_t> packet, std::span<const std::uint32_t> code, const std::vector<std::uint32_t>& userData, const ShaderRecompiler::ShaderComputeStageInfo& compute, const std::shared_ptr<VulkanDevice>& localDevice);
    static bool matchesCopyKernel(std::span<const std::uint32_t> code, const std::vector<std::uint32_t>& userData, const ShaderRecompiler::ShaderComputeStageInfo& compute);
    static void countCopy(int path, std::size_t bytes, std::chrono::steady_clock::time_point started, std::chrono::steady_clock::time_point locked, const VulkanDevice::CopyOutcome* outcome = nullptr);
    void noteCopyWriter(std::uint64_t program, std::uint64_t begin, std::uint64_t end, std::uint32_t queue, std::span<const std::byte> value = {}, std::uint64_t generation = 0);
    static bool copyKnownValues();
    static bool knownValueVerify();
    bool copyBuffer(QueueState& queue, std::uint32_t queueId, std::span<const std::uint32_t> packet, std::span<const std::uint32_t> code, const std::vector<std::uint32_t>& userData, const ShaderRecompiler::ShaderComputeStageInfo& compute, const std::shared_ptr<VulkanDevice>& localDevice, std::uint64_t programAddress);
    void traceCopyRefused(const VulkanDevice::CopyOutcome& outcome, std::uint64_t source, std::uint64_t destination, std::size_t bytes, const std::shared_ptr<VulkanDevice>& localDevice);
    static bool traceCopy();
    void traceCopyPending(const char* what, std::uint64_t address, std::size_t bytes, std::uint64_t source, std::uint64_t destination, std::size_t copyBytes, const std::shared_ptr<VulkanDevice>& localDevice);
    static void countIndirect(int path, double readMs);
    static int indirectArgumentsPath(std::span<const std::uint32_t> packet, std::uint64_t arguments);
    void dispatchIndirect(QueueState& queue, std::span<const std::uint32_t> packet, const Submission& submission);
    enum class AheadArguments { None, Matched, Differed };
    struct IndirectSource {
        std::uint64_t arguments = 0;
        int path = IndirectGpu;
        std::uint32_t queue = 0;
        AheadArguments ahead = AheadArguments::None;
    };
    static IndirectSource& indirectSource();
    static std::string describeIndirectSource();
    std::array<std::uint32_t, 5> readDispatchArguments(std::uint32_t queue, std::uint64_t arguments, std::uint32_t initiator);
    static bool& sampledRead();
    static bool writeEvidenceEnabled();
    static bool writerKeyedEvidence();
    static bool foreignWriters();
    static bool observationGuard();
    static bool siteSampling();
    static std::uint64_t hookWaits();
    static ShaderMemory::HookWaitCounter hookWaitCounter();
    static bool validateSkipEnabled();
    static bool vouchedValidation();
    static bool validateLegacy();
    static bool gateRetry();
    static bool validateSkipVerify();
    static std::uint32_t writeEvidenceAfter();
    static std::uint32_t writeEvidenceSampleEvery();
    static std::uint64_t writeEvidenceMaxBytes();
    static bool traceCapSync();
    void observeDword(std::uint64_t address, bool unchanged);
    void observeRange(std::uint64_t address, std::span<const std::byte> before);
    static void observePendingWrite(std::uint64_t address, bool unchanged);
    static bool knownValueCurrent(const WrittenBuffer& writer);
    ShaderMemory::PendingWrite classifyPendingWrite(std::uint64_t address, std::size_t bytes, std::uint64_t ValidateCounters::*& reason, const PendingView& pending, std::span<std::byte> known = {});
    static ShaderMemory::PendingWrite queryPendingWrite(std::uint64_t address, std::size_t bytes, std::span<std::byte> known);
    template<typename TVisit>
    static void forEachWrittenBuffer(const ShaderRecompiler::RecompileResult& compiled, TVisit&& visit);
    void noteWrittenBuffers(std::uint64_t program, std::uint32_t queue, const ShaderRecompiler::RecompileResult& compiled);
    static void appendWrittenRanges(const ShaderRecompiler::RecompileResult& compiled, std::vector<std::pair<std::uint64_t, std::uint64_t>>& ranges);
    void noteForeignWriter(std::uint64_t begin, std::uint64_t end, std::uint32_t queue);
    void noteDrawWriters(std::span<const Graphics::CompiledShader> stages, std::uint32_t queue);
    static std::vector<std::pair<std::uint64_t, std::uint64_t>> drawWriteRanges(const Graphics::State& graphics, std::span<const Graphics::CompiledShader> stages);
    void commitDraw(std::shared_ptr<VulkanDevice> localDevice, std::uint32_t queue, const Graphics::State& graphics, const Pm4::DrawParameters& drawParameters, std::span<const Graphics::CompiledShader> stages, std::span<const Graphics::GuestMemorySnapshot> snapshots, const std::shared_ptr<const DrawRecipe>& recipe, const std::vector<std::shared_ptr<DispatchVariant>>& recipeStages, std::uint64_t drawKey);
    bool labelPacketQueues(std::span<const std::uint32_t> packet, std::uint32_t opcode);
    bool enqueueLabelPacket(std::span<const std::uint32_t> packet, std::uint32_t opcode, std::uint32_t queue);
    bool enqueueDmaPacket(std::span<const std::uint32_t> packet, std::uint32_t opcode, std::uint32_t queue, const QueueState& state);
    void commitDmaStore(std::uint32_t queue, std::uint64_t address, std::span<const std::byte> bytes);
    void commitLabel(std::uint32_t queue, std::uint64_t address, std::span<const std::byte> bytes, bool endOfPipeInterrupt);
    void submitDueAfterCommit(VulkanDevice* localDevice);
    std::optional<WrittenBuffer> newestWriterLocked(std::uint64_t begin, std::uint64_t end) const;
    std::optional<WrittenBuffer> newestWriter(std::uint64_t begin, std::uint64_t end);
    std::string describeWriters(std::uint64_t begin, std::uint64_t end);
    static std::string describeSelf(const ShaderRecompiler::RecompileResult& compiled, std::uint64_t begin, std::uint64_t end);
    static bool traceBudget();
    void traceCapture(const char* what, std::uint64_t program, std::uint32_t queue, std::span<const ShaderRecompiler::MemoryRegion> regions, double waitedMs);
    void reportValidation(ValidateCounters& counters);
    bool captureStable(std::span<const ShaderRecompiler::MemoryRegion> captured);
    bool validateCaptured(std::uint64_t program, std::uint32_t queue, std::span<const ShaderRecompiler::MemoryRegion> captured, const ShaderRecompiler::RecompileResult& compiled, bool inPlace, const PendingView& view, bool* unmapped = nullptr, std::optional<SampledReadScope>* sampling = nullptr, const DataMask* data = nullptr, std::span<const VouchedRegions> vouched = {});
    static void appendEntryRegions(const DispatchVariant& variant, std::vector<ShaderRecompiler::MemoryRegion>& regions, const std::vector<std::uint32_t>* words = nullptr);
    static std::shared_ptr<const ShaderRecompiler::RecompileResult> patchDataWords(const DispatchVariant& variant, const std::vector<std::uint32_t>& words);
    bool syncPendingRuns(std::uint64_t program, std::uint32_t queue, const ShaderRecompiler::RecompileResult& compiled, std::span<const ShaderRecompiler::MemoryRegion> regions, std::uint64_t& synced, PendingView& pending, std::optional<SampledReadScope>& sampling, std::span<const VouchedRegions> skipped = {});
    EntryOutcome validateVariant(std::uint64_t program, std::uint32_t queue, const DispatchVariant& variant, std::span<const ShaderRecompiler::MemoryRegion> regions, std::uint64_t& imagesFlushed, std::uint64_t& runsSynced, std::optional<SampledReadScope>& sampling, std::vector<std::pair<std::uint32_t, std::uint32_t>>* live = nullptr);
    static int waitTimeoutMs();
    static bool pollReapAll();
    static bool pollTryEach();
    static bool traceLateLabels();
    void waitMemory(std::span<const std::uint32_t> packet, std::uint32_t queue, const PacketHistory& context, std::uint64_t received, bool heldAtSubmit, bool requireMemory = false);
    struct PendingWait {
        std::array<std::uint32_t, 8> words;
        std::size_t count;
        std::uint64_t received;
    };
    static std::vector<PendingWait>& pendingWaits();
    static void notePendingWait(std::span<const std::uint32_t> packet, std::uint32_t queue, std::uint64_t received);
    void landPendingWaits(std::uint32_t queue);
    struct ResolvedDispatch {
        std::uint64_t address = 0, key = 0, indirectArguments = 0;
        std::array<std::uint32_t, 5> packet{};
        std::shared_ptr<const ShaderSnapshot> registeredShader;
        std::shared_ptr<const ShaderRecompiler::RecompileResult> compiledResult;
        std::shared_ptr<DispatchVariant> keepVariant, attachVariant;
        std::shared_ptr<ShaderMemory> shaderMemory;
        std::vector<ShaderRecompiler::MemoryRegion> captured;
        std::vector<std::uint32_t> liveWords;
        std::shared_ptr<const ShaderRecompiler::ResourceCapture> capture;
        bool dataHit = false, cached = false, validated = false;
        std::chrono::steady_clock::time_point resolvedAt{};
    };
    struct GroupCaptureStats {
        std::uint64_t runs = 0, resolved = 0, resolvedHits = 0, adopted = 0, addressMismatches = 0, keyMismatches = 0, memoryMismatches = 0, failures = 0, stale = 0, labelOverlaps = 0, writtenBefore = 0, deviceRejects = 0, argumentMismatches = 0;
        double resolveMs = 0, resolveMaxMs = 0, ageMs = 0, ageMaxMs = 0;
        std::chrono::steady_clock::time_point reported = std::chrono::steady_clock::now();
    };
    static std::map<std::size_t, ResolvedDispatch>& resolvedAhead();
    static GroupCaptureStats& groupCaptureStats();
    static std::size_t& currentPacketOffset();
    static bool& resolvingAhead();
    static bool& gateOpened();
    void resolveGroupAhead(const Submission& submission, const QueueState& live, std::size_t from);
    static void clearResolvedAhead();
    static void reportGroupCapture(std::uint32_t queue);
    static PollStats& pollStats();
    static WaitOutcomes& waitOutcomes();
    static Graphics::Recorder::LateStatistics& lateCountsSeen();
    static EpochBumps& epochBumps();
    static void bumpEpoch(std::uint64_t EpochBumps::*counter);
    static std::chrono::steady_clock::time_point& lastEpochBump();
    static std::deque<const std::uint32_t*>& releasedTails();
    void noteReleasedTails(const Submission& submission);
    static bool packetEpoch();
    static bool labelTryEachPacket();
    static std::chrono::microseconds labelFlushDeadline();
    static bool labelFlushDue();
    static bool completionsPending();
    static bool reapEachPacket();
    static std::uint64_t batchCap();
    void recordDeferredLabels(VulkanDevice* localDevice, std::uint32_t queue);
    bool recordLabelsForPacket(VulkanDevice* localDevice, std::uint32_t queue);
    void recordQueuedLabelsBeforeRead(std::uint32_t queue);
    bool recordQueuedLabelsAfterCapture(std::uint32_t queue, std::span<const ShaderRecompiler::MemoryRegion> regions);
    void noteQueuedLabels(std::uint32_t queue);
    void recordQueuedLabelsByTry(std::uint32_t queue, std::atomic<std::uint64_t>& counter);
    static void recordQueuedLabelsFromHook();
    void flushBetweenPackets(std::uint32_t queue, std::uint32_t header, bool labelPacket);
    bool preparePacketMemory(const Submission& submission, QueueState& queue, std::span<const std::uint32_t> packet, std::uint32_t header, std::uint32_t opcode, bool& wroteOnGpu, bool& endOfPipeInterrupt, bool& interruptDeferred, bool& drawPacket, bool& sampleDump);
    void dumpSampleCounters(std::uint64_t address);
    template <typename TWork>
    static void timed(double WorkerProfile::*bucket, TWork&& work);
    FrameTiming* frameTiming();
    FrameTiming* includeTimingSubmission(const Submission& submission, bool firstSegment);
    void execute(const Submission& submission);
    void deliverGuestEopInterrupt(std::uint32_t queue);
    void deliverFlipEopInterrupt(std::uint32_t queue);
    void markCompleted(std::uint64_t serial);
    static const std::atomic<std::uint64_t>*& workerQueued();
    void reapCompletionLabels();
    void run(std::uint32_t id) noexcept;

    std::mutex mutex;
    AgcDriver::Mutex shutdownMutex;
    std::condition_variable changed;

    std::map<std::uint32_t, QueueWorker> workers;
    std::atomic<std::uint64_t> frameSerial{0};
    std::shared_ptr<FrameTiming> pendingFrameTiming;

    std::atomic<std::uint64_t> flipsCounted{0};
    struct AwaitedSlot {
        std::atomic<std::uint64_t> address{0};
        std::atomic<std::uint64_t> reference{0};
        std::atomic<std::uint64_t> mask{0};
        std::atomic<std::uint64_t> shape{0};
    };
    std::array<AwaitedSlot, SubmissionQueueEnd - 0x20 + 1> awaitedSlots{};
    void noteWaitFields(std::uint32_t queue, std::span<const std::uint32_t> packet);
    std::atomic<std::uint64_t> flipSerial{0};
    std::atomic<std::uint64_t> flipBatchesUnsignaled{0};

    std::mutex eopSelfSignalMutex;
    std::unordered_map<std::uint32_t, std::uint32_t> eopSelfSignalFlips;

    alignas(64) std::atomic<int> packetsInFlight{0};
    std::atomic<std::uint64_t> packetsDone{0};

    std::atomic<std::uint64_t> eventSerial{0};
    alignas(64) std::shared_ptr<ShaderRegistry> shaders = std::make_shared<ShaderRegistry>();

    std::unordered_map<std::uint64_t, std::shared_ptr<DispatchEntry>> dispatchCache;

    std::list<std::uint64_t> dispatchOrder;
    AgcDriver::Mutex dispatchCacheMutex;
    std::uint64_t dispatchCacheHits = 0;
    std::uint64_t dispatchCacheEvictions = 0;

    std::uint64_t dispatchCacheVariants = 0;
    std::uint64_t dispatchCacheVariantBytes = 0;

    EntryCounters entryCounters;

    std::atomic<std::uint64_t> dataPendingMisses{0};

    std::unordered_map<std::uint64_t, std::deque<std::uint64_t>> priorValueSets;

    std::unordered_map<std::uint64_t, std::shared_ptr<DrawEntry>> drawCache;
    std::unordered_map<std::uint64_t, std::uint64_t> drawShapes;
    std::unordered_set<std::uint64_t> traceShapes, traceKeysSeen, traceKeysEvicted;
    std::list<std::uint64_t> drawOrder;
    std::unordered_map<std::uint64_t, std::shared_ptr<const DrawRelocation>> drawRelocations;
    std::unordered_map<std::uint64_t, std::shared_ptr<DrawEntry>> drawAliases;
    DrawRelocationPending drawRelocationPending;
    std::unordered_map<std::uint64_t, DrawRelocationTrial> drawRelocationTrials;
    DrawRelocationCounters drawRelocationCounters;
    AgcDriver::Mutex drawCacheMutex;
    std::uint64_t drawCacheHits = 0, drawCacheEvictions = 0, drawCacheVariants = 0, drawCacheVariantBytes = 0;
    std::optional<DrawKeySightings> drawKeySightings;
    std::optional<DrawKeySightings> drawShapeSightings;

    DrawEntryCounters drawEntryCounters;

    AgcDriver::Mutex driverPhasesMutex;
    std::array<DriverPhaseTotals, DispatchClassCount> driverPhaseTotals{};
    std::chrono::steady_clock::time_point driverPhasesReport = std::chrono::steady_clock::now();

    AgcDriver::Mutex drawPhasesMutex;
    DrawPhaseTotals drawPhaseTotals;

    std::map<std::uint32_t, QueueState> queues;
    std::map<std::uint32_t, std::shared_ptr<IVideoOutput>> outputs;
    struct EopFlip {
        std::uint32_t handle;
        std::shared_ptr<IFlipRequest> request;
    };
    std::map<std::uint32_t, EopFlip> eopFlips;
    std::uint32_t nextEopToken = EopFlipTokenBase;
    DevicePointer device;

    AgcDriver::Mutex labelStoresMutex;
    std::unordered_map<std::uint64_t, std::array<LabelStore, LabelStoreHistory>> labelStores;

    std::vector<std::shared_ptr<VulkanDevice>> replacedDevices;
    std::stop_token shutdownToken = LibcShutdownToken_nid_postfix();
    DeviceUseGate deviceReplacement;
    std::uint64_t accepted = 0;
    std::uint64_t completed = 0;
    std::set<std::uint64_t> completedOutOfOrder;
    std::exception_ptr failure;

    std::atomic<bool> failed{false};
    std::atomic<bool> stopping{false};
    bool stopped = false;
    bool resetGraphics = false;

    std::uint32_t idleWaiters = 0;
    std::uint64_t queue0Executing = 0;
    std::deque<std::uint64_t> queue0Uncommitted;
    std::atomic<std::uint32_t> orderHolders{0};
    std::atomic<std::uint32_t> runningWorkers{0};
    std::atomic<std::uint64_t> idleEpoch{0};
    std::atomic<std::uint64_t> queue0Awaited{0};
    std::atomic<std::uint32_t> flipsAhead{0};
    std::atomic<std::uint32_t> flipHolders{0};
    std::atomic<bool> queue0AwaitsTitle{false};

    std::atomic<std::uint64_t> evidenceReads{0};
    std::atomic<std::uint64_t> evidenceValidations{0};
    std::atomic<std::uint64_t> dispatchesAdoptedAhead{0};
    std::atomic<std::uint64_t> dispatchesRejectedAhead{0};
    std::atomic<void (*)()> aheadAdoptHook{nullptr};

    AgcDriver::Mutex writtenBuffersMutex;
    std::deque<WrittenBuffer> writtenBuffers;
    std::uint64_t writtenBufferSerial = 0;
    std::unordered_map<std::uint64_t, DwordEvidence> dwordEvidence;
    std::atomic<std::uint64_t> observedUnchanged{0};
    std::atomic<std::uint64_t> observedChanged{0};

    std::atomic<std::uint64_t> observationsNoWait{0};
    std::atomic<std::uint64_t> observationsNotReached{0};

    AgcDriver::Mutex validateMutex;
    ValidateCounters validateCounters;

};

}

#endif
