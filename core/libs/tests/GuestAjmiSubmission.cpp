#include "prx/libSceAjmi/libSceAjmi.hpp"
#include "prx/libSceAjm.native/libSceAjm.native.hpp"
#include "tests/CommandLineArgument.hpp"

#include <algorithm>
#include <array>
#include <bit>
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
        std::fprintf(stderr, "AJMI submission check failed at line %u\n", location.line());
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
    decltype(&sceAjmiBatchStartBuffer) submit;
    decltype(&sceAjmBatchStartBuffer) reference;
    decltype(&sceAjmInitialize) initialize;
    decltype(&sceAjmInstanceCreate) create;
    decltype(&sceAjmInstanceDestroy) destroy;
    decltype(&sceAjmBatchInitialize) batchInitialize;
    decltype(&sceAjmBatchJobInitialize) jobInitialize;
    decltype(&sceAjmBatchStart) start;
    decltype(&sceAjmBatchWait) wait;
    decltype(&sceAjmBatchCancel) cancel;
};

template <typename TValue>
struct Guarded {
    std::uint64_t before = 0x0123456789ABCDEFull;
    TValue value{};
    std::uint64_t after = 0xFEDCBA9876543210ull;

    void Check() const {
        Require(before == 0x0123456789ABCDEFull && after == 0xFEDCBA9876543210ull);
    }
};

constexpr int InvalidContext = static_cast<int>(0x80930002u);
constexpr int InvalidBatch = static_cast<int>(0x80930004u);
constexpr int InvalidParameter = static_cast<int>(0x80930005u);
constexpr int MalformedBatch = static_cast<int>(0x80930011u);

std::uint32_t Context(const Api& api) {
    std::uint32_t context = 0;
    Require(api.initialize(0, &context) == 0 && context != 0);
    return context;
}

using Words = std::array<std::uint32_t, 16>;

void Buffer(Words& batch, std::size_t word, std::uint32_t kind, const void* pointer, std::size_t size) {
    batch[word] = kind;
    batch[word + 1] = static_cast<std::uint32_t>(size);
    const auto address = reinterpret_cast<std::uintptr_t>(pointer);
    batch[word + 2] = static_cast<std::uint32_t>(address);
    batch[word + 3] = static_cast<std::uint32_t>(address >> 32);
}

const std::uint8_t* Data(const Words& batch) {
    return reinterpret_cast<const std::uint8_t*>(batch.data());
}

void InitializePcm(const Api& api, std::uint32_t context, std::uint32_t instance, std::uint32_t channels) {
    std::array<std::uint8_t, 256> storage{};
    const std::array<std::uint32_t, 3> parameters{0, channels, 48000};
    AjmBatchInfo info{};
    std::array<std::int32_t, 2> result{-1, -1};
    std::uint32_t id = 0;
    Require(api.batchInitialize(storage.data(), storage.size(), &info) == 0);
    Require(api.jobInitialize(&info, instance, parameters.data(), sizeof(parameters), result.data()) == 0);
    Require(api.start(context, &info, SCE_AJM_PRIORITY_MIN, nullptr, &id) == 0);
    Require(api.wait(context, id, 0, nullptr) == 0 && result[0] == 0 && result[1] == 0);
}

struct StreamResult {
    std::int32_t result;
    std::int32_t internalResult;
    std::int32_t consumed;
    std::int32_t written;
    std::uint64_t samples;
};

static_assert(sizeof(StreamResult) == 24);

void CheckExecute(const Api& api) {
    const auto context = Context(api);
    const auto other = Context(api);
    const std::array<std::int16_t, 16> input{-32768, 32767, -12345, 12345, -1, 0, 1, 2, -2000, 3000, -4000, 5000, 6000, -7000, 8000, -9000};
    for (const auto channels : {1u, 2u, 4u, 8u}) {
        std::uint32_t instance = 0;
        Require(api.create(context, 23, 0, &instance) == 0);
        InitializePcm(api, context, instance, channels);
        Guarded<std::array<std::int16_t, 16>> output;
        output.value.fill(0x1234);
        Guarded<StreamResult> stream;
        std::memset(&stream.value, 0xA5, sizeof(stream.value));
        Guarded<AjmBatchError> error;
        std::memset(&error.value, 0xA5, sizeof(error.value));
        Guarded<std::uint32_t> id;
        Words batch{};
        batch[0] = instance << 6;
        batch[1] = 56;
        Buffer(batch, 2, 1, input.data(), sizeof(input));
        batch[6] = 4 | (0x8000u << 6);
        batch[7] = 0;
        Buffer(batch, 8, 17, output.value.data(), sizeof(output.value));
        Buffer(batch, 12, 18, &stream.value, sizeof(stream.value));
        const auto original = batch;
        Require(api.submit(context, Data(batch), sizeof(batch), SCE_AJM_PRIORITY_MIN, &error.value, &id.value) == 0 && id.value != 0);
        Require(output.value == input && batch == original);
        Require(stream.value.result == 0 && stream.value.internalResult == 0);
        Require(stream.value.consumed == sizeof(input) && stream.value.written == sizeof(input));
        Require(stream.value.samples == input.size() / channels);
        const std::array<std::uint8_t, sizeof(AjmBatchError)> zero{};
        Require(std::memcmp(zero.data(), &error.value, zero.size()) == 0);
        Require(api.wait(other, id.value, 0, nullptr) == InvalidBatch);
        Require(api.cancel(context, id.value) == 0);
        Require(api.wait(context, id.value, 0, &error.value) == 0);
        Require(api.wait(context, id.value, 0, nullptr) == InvalidBatch);
        Require(api.destroy(context, instance) == 0);
        output.Check();
        stream.Check();
        error.Check();
        id.Check();
    }
}

void CheckErrors(const Api& api) {
    const auto context = Context(api);
    Words batch{};
    batch[0] = 7;
    Guarded<std::uint32_t> id;
    id.value = 0xA5A5A5A5u;
    Guarded<AjmBatchError> error;
    std::memset(&error.value, 0xA5, sizeof(error.value));
    std::array<std::uint8_t, sizeof(AjmBatchError)> unchanged{};
    std::memcpy(unchanged.data(), &error.value, unchanged.size());
    Require(api.submit(0, nullptr, 0, 0, &error.value, &id.value) == InvalidContext);
    Require(api.submit(std::numeric_limits<std::uint32_t>::max(), Data(batch), sizeof(batch), 0, &error.value, &id.value) == InvalidContext);
    Require(api.submit(context, nullptr, 0, 0, &error.value, &id.value) == InvalidParameter);
    Require(api.submit(context, Data(batch), sizeof(batch), 0, &error.value, nullptr) == InvalidParameter);
    Require(id.value == 0xA5A5A5A5u && std::memcmp(unchanged.data(), &error.value, unchanged.size()) == 0);
    for (const auto size : {1u, 7u, 9u, 12u, 63u}) {
        AjmBatchError expected{};
        std::uint32_t expectedId = 0xA5A5A5A5u;
        const auto status = api.reference(context, Data(batch), size, 0, &expected, &expectedId);
        Require(status == MalformedBatch);
        Require(api.submit(context, Data(batch), size, 0, &error.value, &id.value) == status);
        Require(error.value.error_code == expected.error_code && error.value.job_addr == expected.job_addr);
        Require(error.value.cmd_offset == expected.cmd_offset && error.value.job_ra == expected.job_ra);
        Require(id.value == expectedId && id.value == 0xA5A5A5A5u);
    }
    for (const auto priority : {SCE_AJM_PRIORITY_MIN, SCE_AJM_PRIORITY_MAX}) {
        Require(api.submit(context, Data(batch), 8, priority, nullptr, &id.value) == 0 && id.value != 0);
        Require(api.wait(context, id.value, 0, nullptr) == 0);
    }
    id.Check();
    error.Check();
}

void CheckUnsupported(const Api& api) {
    const auto context = Context(api);
    Words batch{};
    batch[0] = 5;
    const auto original = batch;
    const auto rejection = [&](decltype(api.submit) call) {
        std::uint32_t id = 0xA5A5A5A5u;
        std::string message;
        try {
            call(context, Data(batch), 8, 0, nullptr, &id);
        } catch (const std::exception& error) {
            message = error.what();
        }
        Require(!message.empty() && id == 0xA5A5A5A5u && batch == original);
        return message;
    };
    Require(rejection(api.submit) == rejection(api.reference));
}

} // namespace

int main(int argc, char** argv) {
    Require(argc == 4);
    const std::filesystem::path directory = ArgumentPath(argv, 1);
    const bool patched = std::string_view(argv[3]) == "nid";
    Require(patched || std::string_view(argv[3]) == "unpatched");
    Module native(directory / "libSceAjm.native.prx");
    Module alias(directory / "libSceAjmi.prx");
    const Api api{
        alias.Import<decltype(Api::submit)>(patched ? "9zA4YMlkR9M" : "sceAjmiBatchStartBuffer"),
        native.Import<decltype(Api::reference)>(patched ? "fFFkk0xfGWs" : "sceAjmBatchStartBuffer"),
        native.Import<decltype(Api::initialize)>(patched ? "dl+4eHSzUu4" : "sceAjmInitialize"),
        native.Import<decltype(Api::create)>(patched ? "AxoDrINp4J8" : "sceAjmInstanceCreate"),
        native.Import<decltype(Api::destroy)>(patched ? "RbLbuKv8zho" : "sceAjmInstanceDestroy"),
        native.Import<decltype(Api::batchInitialize)>(patched ? "MmpF1XsQiHw" : "sceAjmBatchInitialize"),
        native.Import<decltype(Api::jobInitialize)>(patched ? "ezM2OhNxzck" : "sceAjmBatchJobInitialize"),
        native.Import<decltype(Api::start)>(patched ? "5tOfnaClcqM" : "sceAjmBatchStart"),
        native.Import<decltype(Api::wait)>(patched ? "-qLsfDAywIY" : "sceAjmBatchWait"),
        native.Import<decltype(Api::cancel)>(patched ? "NVDXiUesSbA" : "sceAjmBatchCancel")
    };
    const std::string_view operation = argv[2];
    if (operation == "execute") CheckExecute(api);
    else if (operation == "errors") CheckErrors(api);
    else {
        Require(operation == "unsupported");
        CheckUnsupported(api);
    }
}
