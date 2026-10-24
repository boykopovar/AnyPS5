#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_SHADERREGISTRY_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_SHADERREGISTRY_HPP

#include "prx/libSceAgcDriver/Execution/include/ShaderMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/QueueState.hpp"
#include <array>
#include <optional>
#include <span>
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

struct Shader;

namespace ShaderRecompiler {
class ShaderPreparationContext;
struct LinkedCalls;
}

namespace AgcDriver {
class VulkanDevice;
}

namespace AgcDriver::DriverDetail {

struct ShaderSnapshot;

struct UserDataCallSites;
class ShaderRegistry {
    using Chunk = std::map<std::uint64_t, std::vector<std::shared_ptr<const ShaderSnapshot>>>;
    using Chunks = std::vector<std::shared_ptr<Chunk>>;

public:
    using key_type = std::uint64_t;
    using mapped_type = Chunk::mapped_type;
    using value_type = Chunk::value_type;

    class const_iterator {
    public:
        using iterator_category = std::bidirectional_iterator_tag;
        using value_type = ShaderRegistry::value_type;
        using difference_type = std::ptrdiff_t;
        using pointer = const value_type*;
        using reference = const value_type&;

        const_iterator() = default;
        reference operator*() const { return *entry; }
        pointer operator->() const { return &*entry; }
        const_iterator& operator++() {
            if (++entry == (*chunk)->end()) {
                ++chunk;
                entry = chunk == last ? Chunk::const_iterator{} : (*chunk)->cbegin();
            }
            return *this;
        }
        const_iterator operator++(int) {
            auto previous = *this;
            ++*this;
            return previous;
        }
        const_iterator& operator--() {
            if (chunk == last || entry == (*chunk)->cbegin()) {
                --chunk;
                entry = std::prev((*chunk)->cend());
            } else {
                --entry;
            }
            return *this;
        }
        const_iterator operator--(int) {
            auto previous = *this;
            --*this;
            return previous;
        }
        bool operator==(const const_iterator& other) const { return chunk == other.chunk && entry == other.entry; }

    private:
        friend class ShaderRegistry;
        const_iterator(Chunks::const_iterator chunk, Chunks::const_iterator last, Chunk::const_iterator entry) : chunk(chunk), last(last), entry(entry) {}
        Chunks::const_iterator chunk{};
        Chunks::const_iterator last{};
        Chunk::const_iterator entry{};
    };
    using iterator = const_iterator;

    ShaderRegistry() = default;
    ShaderRegistry(std::initializer_list<value_type> entries) {
        for (const auto& [address, headers] : entries) emplace(address, headers);
    }

    const_iterator begin() const { return chunks.empty() ? end() : const_iterator(chunks.cbegin(), chunks.cend(), chunks.front()->cbegin()); }
    const_iterator end() const { return const_iterator(chunks.cend(), chunks.cend(), {}); }
    const_iterator find(std::uint64_t address) const;
    const_iterator upper_bound(std::uint64_t address) const;
    bool contains(std::uint64_t address) const { return find(address) != end(); }
    const mapped_type& at(std::uint64_t address) const;
    std::size_t size() const { return count; }
    bool empty() const { return count == 0; }
    mapped_type& operator[](std::uint64_t address);
    std::pair<const_iterator, bool> emplace(std::uint64_t address, mapped_type headers);
    std::size_t erase(std::uint64_t address);
    void clear() {
        chunks.clear();
        count = 0;
    }

private:
    static constexpr std::size_t ChunkEntries = 64;

    std::size_t chunkFor(std::uint64_t address) const;
    Chunk& writable(std::size_t index);

    Chunks chunks;
    std::size_t count = 0;
};

struct PreparedShaderState {
    struct Entry {
        std::size_t codeOffset;
        std::shared_ptr<const ShaderRecompiler::SourceHandle> handle;
        bool fromDeferredAbi = false;
        bool matched = false;
    };
    struct Rectangle {
        std::uint64_t vertexId;
        std::uint64_t fragmentId;
        std::shared_ptr<const ShaderRecompiler::RectListShaders> shaders;
    };
    struct RectangleProgress {
        std::weak_ptr<const ShaderSnapshot> fragment;
        std::size_t vertexCount;
        std::size_t fragmentCount;
    };
    struct GraphicsAbi {
        std::vector<std::uint64_t> key;
        std::weak_ptr<const ShaderRegistry> registry;
        std::vector<std::weak_ptr<const ShaderSnapshot>> stages;
    };
    struct UserDataLink {
        std::size_t codeOffset = 0;
        std::uint32_t userDataBaseRegister = 0;
        std::uint32_t userDataCount = 0;
        std::uint32_t waveSize = 0;
        std::shared_ptr<const UserDataCallSites> sites;
        std::vector<std::uint64_t> callees;
        std::vector<std::vector<std::uint32_t>> code;
        std::shared_ptr<const ShaderRecompiler::LinkedCalls> linked;
    };
    std::vector<Entry> entries;
    std::vector<UserDataLink> userDataLinks;
    std::vector<std::vector<std::uint64_t>> registeredAbis;
    std::vector<GraphicsAbi> graphicsAbis;
    std::vector<Rectangle> rectangles;
    std::vector<RectangleProgress> rectangleProgress;
    std::vector<std::weak_ptr<const ShaderSnapshot>> fragments;
    bool rectangleRequested = false;
    bool deferred = false;
};
struct PreparedShaders : PreparedShaderState {
    struct DeferredAbi {
        std::vector<std::uint64_t> key;
        std::vector<std::pair<std::uint32_t, std::uint32_t>> context;
        std::vector<std::pair<std::uint32_t, std::uint32_t>> primitive;
        std::weak_ptr<VulkanDevice> device;
    };
    std::vector<DeferredAbi> deferredAbis;
    bool deferredPreparing = false;
    std::mutex mutex;
    std::condition_variable settled;
    std::exception_ptr failure;
    std::atomic<bool> demanded{false};
    bool pending = false;
    std::vector<std::size_t> convertedSamples;
};

struct RegisteredShaderState {
    Registers shader;
    Registers context;
    Registers userConfig;

    bool operator==(const RegisteredShaderState&) const = default;
};

struct ShaderSnapshot {
    std::uint64_t codeAddress;
    std::uint64_t headerAddress;
    std::uint8_t type;
    std::vector<std::uint32_t> code;
    std::vector<std::byte> header;
    std::shared_ptr<PreparedShaders> prepared = std::make_shared<PreparedShaders>();
    std::shared_ptr<const RegisteredShaderState> registeredState;
    std::shared_ptr<std::atomic<std::uint8_t>> workgroupUse = std::make_shared<std::atomic<std::uint8_t>>(0u);
    std::shared_ptr<std::atomic<std::uint8_t>> positionVectorUse = std::make_shared<std::atomic<std::uint8_t>>(0u);
    std::shared_ptr<std::atomic<std::uint8_t>> memoryWrites = std::make_shared<std::atomic<std::uint8_t>>(0u);
    std::shared_ptr<std::atomic<bool>> rawGraphicsNoted = std::make_shared<std::atomic<bool>>(false);
    std::optional<std::uint32_t> userDataTailRegister;
};


std::shared_ptr<const ShaderSnapshot> ReadRawShader(std::uint64_t address);
std::shared_ptr<const ShaderSnapshot> ReadUserDataTailContinuation(std::span<const std::uint32_t> code, std::uint32_t userDataBase, std::span<const std::uint32_t> userData);
std::shared_ptr<const ShaderSnapshot> ProgramSnapshot(const ShaderRegistry& shaders, std::uint64_t address, std::initializer_list<std::uint8_t> types);
void NoteRawGraphicsDraw(const ShaderSnapshot& snapshot);
bool RawGraphicsDrawn();
bool RawGraphicsDrawnIn(std::uint64_t begin, std::uint64_t end);
bool ProgramUsesWorkgroup(const ShaderSnapshot& snapshot, std::size_t codeOffset);
bool ProgramWritesMemory(const ShaderSnapshot& snapshot, std::size_t codeOffset);
// The PA_CL_VS_OUT_CNTL position export bits a vertex program is compiled with: none for a program
// without pos1..pos3 exports, so its artifact does not depend on them.
std::uint32_t PositionExportLayout(const ShaderSnapshot* snapshot, std::size_t codeOffset, std::span<const std::uint32_t> code, std::uint32_t paClVsOutCntl, const ShaderSnapshot* continuation = nullptr);
bool VertexWorkgroupRequired(const QueueState& queue, const ShaderRegistry& registry);

std::shared_ptr<const ShaderRecompiler::SourceHandle> PrepareShaderWithDiagnostics(const ShaderRecompiler::RecompileRequest& request, ShaderRecompiler::ShaderPreparationContext* preparation = nullptr);

std::uint64_t NullPixelProgramAddress();
ShaderSnapshot PrepareNullPixelProgram(const VulkanDevice& device);
std::optional<ShaderRecompiler::ShaderFloatMode> RegisteredFloatMode(const ShaderSnapshot& snapshot);
std::optional<ShaderRecompiler::ShaderFloatMode> DispatchFloatMode(const ShaderSnapshot& snapshot, const Registers& live);
std::shared_ptr<const ShaderSnapshot> RegisteredHeader(const ShaderRegistry& registry, const Shader* shader);
std::shared_ptr<const ShaderSnapshot> RegisteredProgram(const ShaderRegistry& registry, std::uint64_t address, std::initializer_list<std::uint8_t> types);
void PublishRegisteredShader(std::shared_ptr<ShaderRegistry>& registry, const std::shared_ptr<const ShaderSnapshot>& snapshot);
void SubmitRegistrationPreparation(std::function<void()> task, const PreparedShaders* prepared = nullptr);
void PrioritizeRegistrationPreparation(PreparedShaders& prepared);
void AwaitRegistrationPreparation();
void SubmitLibraryPrebuild(std::function<void()> task);
void AwaitLibraryPrebuilds();
void CloseRegistrationPreparation();
unsigned RegistrationPreparationThreads();

void ResolvePreparedGraphics(const ShaderSnapshot& front, const std::shared_ptr<const ShaderSnapshot>& fragment, std::uint32_t primitiveType, const ShaderRecompiler::SpirvTarget& target, bool deferUnprepared = false);

ShaderRecompiler::RectListShaders PreparedRectangle(const ShaderSnapshot& snapshot, std::uint64_t vertexId, std::uint64_t fragmentId);
ShaderRecompiler::RectListShaders DrawRectangle(const ShaderSnapshot& front, const std::shared_ptr<const ShaderSnapshot>& fragment, std::uint64_t vertexId, std::uint64_t fragmentId, const ShaderRecompiler::SpirvTarget& target);

std::shared_ptr<const ShaderRecompiler::SourceHandle> SourceHandleFor(const ShaderSnapshot& snapshot, std::size_t codeOffset, const ShaderRecompiler::RecompileRequest& request);
ShaderRecompiler::PreparedShaderInvocation InvocationFor(const ShaderSnapshot& snapshot, std::size_t codeOffset, const ShaderRecompiler::RecompileRequest& request);
std::shared_ptr<const ShaderRecompiler::ResourceCapture> CaptureInvocation(ShaderMemory& memory, const ShaderSnapshot& snapshot, std::size_t codeOffset, const ShaderRecompiler::RecompileRequest& request, ShaderRecompiler::PreparedShaderInvocation& invocation);

}

#endif
