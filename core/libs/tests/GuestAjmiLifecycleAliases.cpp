#include "prx/libSceAjmi/libSceAjmi.hpp"
#include "prx/libSceAjm.native/libSceAjm.native.hpp"

#include <array>
#include <bit>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <limits>
#include <source_location>
#include <string>
#include <string_view>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "AJMI lifecycle alias check failed at line %u\n", location.line());
        std::abort();
    }
}

class Module {
public:
    explicit Module(const std::filesystem::path& path) {
#ifdef _WIN32
        handle = LoadLibraryW(path.c_str());
        if (!handle) std::fprintf(stderr, "LoadLibraryW failed: %lu\n", GetLastError());
#else
        handle = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
        if (!handle) std::fprintf(stderr, "%s\n", dlerror());
#endif
        Require(handle != nullptr);
    }

    ~Module() {
#ifdef _WIN32
        FreeLibrary(handle);
#else
        dlclose(handle);
#endif
    }

    template <typename TFunction>
    TFunction Import(const char* name) const {
#ifdef _WIN32
        auto symbol = GetProcAddress(handle, name);
#else
        void* symbol = dlsym(handle, name);
#endif
        if (!symbol) std::fprintf(stderr, "Missing symbol: %s\n", name);
        Require(symbol != nullptr);
        return std::bit_cast<TFunction>(symbol);
    }

private:
#ifdef _WIN32
    HMODULE handle = nullptr;
#else
    void* handle = nullptr;
#endif
};

struct Api {
    decltype(&sceAjmiInitialize) initialize;
    decltype(&sceAjmiFinalize) finalize;
    decltype(&sceAjmiInstanceCreate) create;
    decltype(&sceAjmiInstanceDestroy) destroy;
    decltype(&sceAjmiBatchWait) wait;
    decltype(&sceAjmiBatchCancel) cancel;
    decltype(&sceAjmiModuleRegister) moduleRegister;
    decltype(&sceAjmiModuleUnregister) moduleUnregister;
};

struct BatchApi {
    decltype(&sceAjmBatchInitialize) initialize;
    decltype(&sceAjmBatchJobGetStatistics) statistics;
    decltype(&sceAjmBatchStart) start;
    decltype(&sceAjmiBatchErrorDump) errorDumpAlias;
    decltype(&sceAjmBatchErrorDump) errorDumpNative;
};

template <typename TValue>
struct Guarded {
    std::uint64_t before = 0x123456789ABCDEF0ull;
    TValue value{};
    std::uint64_t after = 0xFEDCBA9876543210ull;

    void Check() const {
        Require(before == 0x123456789ABCDEF0ull && after == 0xFEDCBA9876543210ull);
    }
};

constexpr int InvalidContext = static_cast<int>(0x80930002u);
constexpr int InvalidInstance = static_cast<int>(0x80930003u);
constexpr int InvalidBatch = static_cast<int>(0x80930004u);
constexpr int InvalidParameter = static_cast<int>(0x80930005u);
constexpr int InvalidFlags = static_cast<int>(0x8093000Cu);

std::uint32_t Context(const Api& api, std::int64_t reserved = 0) {
    Guarded<std::uint32_t> context;
    errno = 71;
    Require(api.initialize(reserved, &context.value) == 0 && context.value != 0);
    Require(errno == 71);
    context.Check();
    return context.value;
}

void CheckContexts(const Api& alias, const Api& native) {
    Require(alias.initialize(0, nullptr) == InvalidParameter);
    errno = 71;
    Require(alias.initialize(0, reinterpret_cast<std::uint32_t*>(0x10)) == InvalidParameter && errno == 71);
    const auto first = Context(alias);
    const auto second = Context(native);
    const auto third = Context(alias, 0x300000000ll);
    const auto fourth = Context(alias, std::numeric_limits<std::int64_t>::min());
    Require(first != second && second != third && third != fourth && first != third && first != fourth && second != fourth);
    Guarded<std::uint32_t> instance;
    Require(native.create(first, 0, 0, &instance.value) == 0);
    Require(alias.destroy(first, instance.value) == 0);
    Require(alias.create(second, 0, 0, &instance.value) == 0);
    Require(native.destroy(second, instance.value) == 0);
    instance.Check();
    errno = 71;
    Require(alias.moduleRegister(first, 0, 1) == InvalidParameter);
    Require(alias.moduleRegister(first, 0, -1) == InvalidParameter);
    Require(alias.moduleRegister(first, 0, 0x100000000ll) == InvalidParameter);
    Require(alias.moduleRegister(first, 0, 0) == 0);
    Require(alias.moduleUnregister(first, 0) == 0);
    Require(alias.moduleRegister(second, 0x24, 0) == 0);
    Require(alias.moduleUnregister(second, 0x24) == 0);
    Require(alias.moduleRegister(first, 1, 0) == 0);
    Require(native.moduleUnregister(first, 1) == 0);
    Require(native.moduleRegister(second, 1, 0) == 0);
    Require(alias.moduleUnregister(second, 1) == 0);
    Require(alias.finalize(first) == 0 && errno == 71);
    Require(native.finalize(second) == 0);
    Require(alias.finalize(third) == 0);
    Require(alias.finalize(fourth) == 0);
    Require(alias.finalize(0) == 0);
    Require(native.finalize(0) == 0);
}

void CheckInstances(const Api& alias, const Api& native, decltype(&sceAjmiInstanceCodecType) codecType) {
    Require(codecType(0) == 0);
    Require(codecType(0x3fffu) == 0);
    Require(codecType(0x4000u) == 1);
    Require(codecType(0xffffffffu) == 0x3ffffu);
    const auto context = Context(alias);
    const auto other = Context(native);
    Guarded<std::uint32_t> first;
    Guarded<std::uint32_t> second;
    first.value = 0xA5A5A5A5u;
    Require(alias.create(0, 0, 0, &first.value) == InvalidContext);
    Require(alias.create(std::numeric_limits<std::uint32_t>::max(), 0, 0, &first.value) == InvalidContext);
    Require(first.value == 0xA5A5A5A5u);
    Require(alias.create(0, 0, 0, nullptr) == InvalidParameter);
    Require(alias.create(context, 0, 0, nullptr) == InvalidParameter);
    errno = 71;
    Require(alias.create(context, 0, 0, reinterpret_cast<std::uint32_t*>(0x10)) == InvalidParameter && errno == 71);
    Require(alias.create(context, 24, 0x1002, &first.value) == 0);
    Require(codecType(first.value) == 24);
    Require(native.create(other, 24, 0x1002, &second.value) == 0 && first.value != second.value);
    Require(native.destroy(other, first.value) == InvalidInstance);
    Require(alias.destroy(context, second.value) == InvalidInstance);
    Require(alias.destroy(0, first.value) == InvalidContext);
    Require(alias.destroy(context, 0) == InvalidInstance);
    Require(native.destroy(context, first.value) == 0);
    Require(alias.destroy(context, first.value) == InvalidInstance);
    Require(alias.destroy(other, second.value) == 0);
    Require(native.destroy(other, second.value) == InvalidInstance);
    Require(alias.destroy(std::numeric_limits<std::uint32_t>::max(), 0) == InvalidContext);
    for (const auto flags : {std::uint64_t{1} << 48, std::uint64_t{1} << 59}) {
        const auto rejected = [&](const Api& api) {
            std::uint32_t instance = 0xA5A5A5A5u;
            std::string message;
            try {
                api.create(context, 2, flags, &instance);
            } catch (const std::exception& error) {
                message = error.what();
            }
            Require(!message.empty() && instance == 0xA5A5A5A5u);
            return message;
        };
        Require(rejected(alias) == rejected(native));
    }
    std::uint32_t refused = 0xA5A5A5A5u;
    Require(alias.create(context, 2, std::uint64_t{1} << 63, &refused) == InvalidFlags && refused == 0xA5A5A5A5u);
    first.Check();
    second.Check();
}

std::uint32_t StartBatch(const BatchApi& api, std::uint32_t context) {
    std::array<std::uint8_t, 256> storage{};
    std::array<std::uint8_t, 24> statistics{};
    AjmBatchInfo info{};
    AjmBatchError error{};
    std::uint32_t id = 0;
    Require(api.initialize(storage.data(), storage.size(), &info) == 0);
    Require(api.statistics(&info, 1.0f, statistics.data()) == 0);
    Require(api.start(context, &info, SCE_AJM_PRIORITY_MIN, &error, &id) == 0 && id != 0);
    return id;
}

void CheckBatches(const Api& alias, const Api& native, const BatchApi& batch) {
    const auto context = Context(alias);
    const auto other = Context(native);
    Guarded<AjmBatchError> error;
    std::memset(&error.value, 0xA5, sizeof(error.value));
    std::array<std::uint8_t, sizeof(AjmBatchError)> unchanged{};
    std::memcpy(unchanged.data(), &error.value, unchanged.size());
    const auto first = StartBatch(batch, context);
    const auto second = StartBatch(batch, other);
    Require(first != second);
    Require(alias.wait(0, first, 0, &error.value) == InvalidContext);
    Require(alias.cancel(0, first) == InvalidContext);
    Require(alias.wait(other, first, 0, &error.value) == InvalidBatch);
    Require(alias.cancel(other, first) == InvalidBatch);
    Require(alias.wait(context, 0, 0, &error.value) == InvalidBatch);
    Require(alias.cancel(context, 0) == InvalidBatch);
    Require(alias.wait(std::numeric_limits<std::uint32_t>::max(), first, 0, &error.value) == InvalidContext);
    Require(alias.cancel(std::numeric_limits<std::uint32_t>::max(), first) == InvalidContext);
    Require(std::memcmp(unchanged.data(), &error.value, unchanged.size()) == 0);
    Require(alias.cancel(context, first) == 0);
    Require(native.cancel(context, first) == 0);
    Require(alias.wait(context, first, std::numeric_limits<std::uint32_t>::max(), &error.value) == 0);
    const std::array<std::uint8_t, sizeof(AjmBatchError)> cleared{};
    Require(std::memcmp(cleared.data(), &error.value, cleared.size()) == 0);
    Require(native.wait(context, first, 0, nullptr) == InvalidBatch);
    Require(alias.cancel(context, first) == InvalidBatch);
    Require(alias.wait(context, first, 0, nullptr) == InvalidBatch);
    Require(native.wait(other, second, 0, &error.value) == 0);
    Require(alias.wait(other, second, 0, &error.value) == InvalidBatch);
    const auto third = StartBatch(batch, context);
    Require(native.cancel(context, third) == 0);
    Require(alias.wait(context, third, 0, nullptr) == 0);
    Require(alias.cancel(context, third) == InvalidBatch);
    const auto fourth = StartBatch(batch, context);
    Require(alias.cancel(context, fourth) == 0);
    Require(native.wait(context, fourth, 0, nullptr) == 0);
    AjmBatchInfo dumpInfo{};
    std::memset(&dumpInfo, 0xAA, sizeof(dumpInfo));
    std::memset(&error.value, 0xAA, sizeof(error.value));
    errno = 71;
    Require(batch.errorDumpAlias(&dumpInfo, &error.value) == 0);
    Require(std::memcmp(cleared.data(), &error.value, cleared.size()) == 0);
    Require(batch.errorDumpAlias(reinterpret_cast<const AjmBatchInfo*>(0x10), nullptr) == 0);
    Require(batch.errorDumpAlias(&dumpInfo, reinterpret_cast<AjmBatchError*>(0x10)) == InvalidParameter);
    std::memset(&error.value, 0xAA, sizeof(error.value));
    Require(batch.errorDumpNative(&dumpInfo, &error.value) == 0);
    Require(std::memcmp(cleared.data(), &error.value, cleared.size()) == 0);
    Require(batch.errorDumpNative(reinterpret_cast<const AjmBatchInfo*>(0x10), nullptr) == 0);
    Require(batch.errorDumpNative(&dumpInfo, reinterpret_cast<AjmBatchError*>(0x10)) == InvalidParameter);
    Require(errno == 71);
    error.Check();
}

} // namespace

int main(int argc, char** argv) {
    Require(argc == 4);
    const std::filesystem::path directory = std::u8string(reinterpret_cast<const char8_t*>(argv[1]));
    const bool patched = std::string_view(argv[3]) == "nid";
    Require(patched || std::string_view(argv[3]) == "unpatched");
    Module nativeModule(directory / "libSceAjm.native.prx");
    Module aliasModule(directory / "libSceAjmi.prx");
    const Api alias{
        aliasModule.Import<decltype(Api::initialize)>(patched ? "QhA7rTjJbro" : "sceAjmiInitialize"),
        aliasModule.Import<decltype(Api::finalize)>(patched ? "BD002VBfhuE" : "sceAjmiFinalize"),
        aliasModule.Import<decltype(Api::create)>(patched ? "ofRX8Vq0ss0" : "sceAjmiInstanceCreate"),
        aliasModule.Import<decltype(Api::destroy)>(patched ? "YtubBgSeNH0" : "sceAjmiInstanceDestroy"),
        aliasModule.Import<decltype(Api::wait)>(patched ? "JJ9g04D344Q" : "sceAjmiBatchWait"),
        aliasModule.Import<decltype(Api::cancel)>(patched ? "6lWAGxD8NLs" : "sceAjmiBatchCancel"),
        aliasModule.Import<decltype(Api::moduleRegister)>(patched ? "dmwcwCeItmE" : "sceAjmiModuleRegister"),
        aliasModule.Import<decltype(Api::moduleUnregister)>(patched ? "6fY-ci3au2U" : "sceAjmiModuleUnregister")
    };
    const Api native{
        nativeModule.Import<decltype(Api::initialize)>(patched ? "dl+4eHSzUu4" : "sceAjmInitialize"),
        nativeModule.Import<decltype(Api::finalize)>(patched ? "MHur6qCsUus" : "sceAjmFinalize"),
        nativeModule.Import<decltype(Api::create)>(patched ? "AxoDrINp4J8" : "sceAjmInstanceCreate"),
        nativeModule.Import<decltype(Api::destroy)>(patched ? "RbLbuKv8zho" : "sceAjmInstanceDestroy"),
        nativeModule.Import<decltype(Api::wait)>(patched ? "-qLsfDAywIY" : "sceAjmBatchWait"),
        nativeModule.Import<decltype(Api::cancel)>(patched ? "NVDXiUesSbA" : "sceAjmBatchCancel"),
        nativeModule.Import<decltype(Api::moduleRegister)>(patched ? "Q3dyFuwGn64" : "sceAjmModuleRegister"),
        nativeModule.Import<decltype(Api::moduleUnregister)>(patched ? "Wi7DtlLV+KI" : "sceAjmModuleUnregister")
    };
    const std::string_view operation = argv[2];
    if (operation == "contexts") {
        CheckContexts(alias, native);
    } else if (operation == "instances") {
        const auto codecType = aliasModule.Import<decltype(&sceAjmiInstanceCodecType)>(patched ? "Uqtj4RSP-JQ" : "sceAjmiInstanceCodecType");
        CheckInstances(alias, native, codecType);
    } else {
        Require(operation == "batches");
        const BatchApi batch{
            nativeModule.Import<decltype(BatchApi::initialize)>(patched ? "MmpF1XsQiHw" : "sceAjmBatchInitialize"),
            nativeModule.Import<decltype(BatchApi::statistics)>(patched ? "3cAg7xN995U" : "sceAjmBatchJobGetStatistics"),
            nativeModule.Import<decltype(BatchApi::start)>(patched ? "5tOfnaClcqM" : "sceAjmBatchStart"),
            aliasModule.Import<decltype(BatchApi::errorDumpAlias)>(patched ? "RbLGSlJB6dc" : "sceAjmiBatchErrorDump"),
            nativeModule.Import<decltype(BatchApi::errorDumpNative)>(patched ? "WfAiBW8Wcek" : "sceAjmBatchErrorDump")
        };
        CheckBatches(alias, native, batch);
    }
}
