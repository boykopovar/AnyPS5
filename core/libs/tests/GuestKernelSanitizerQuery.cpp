#include "prx/libkernel/include/Api.hpp"
#include "SceTypes.hpp"
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "sanitizer query check failed at %s:%u\n", location.file_name(), static_cast<unsigned>(location.line()));
        std::abort();
    }
}

void AuditAddressSanitizerDisabledQuery() {
    Require(sceKernelIsAddressSanitizerEnabled() == 0);
    for (int iteration = 0; iteration < 100; ++iteration) {
        const int enabled = sceKernelIsAddressSanitizerEnabled();
        Require(enabled == 0);
    }
}

void AuditThreadSanitizerDisabledQuery() {
    Require(sceKernelIsThreadSanitizerEnabled() == 0);
    for (int iteration = 0; iteration < 100; ++iteration) {
        const int enabled = sceKernelIsThreadSanitizerEnabled();
        Require(enabled == 0);
    }
}

struct WorkerContext {
    std::atomic<bool> failed{false};
    std::atomic<int> completedIterations{0};
};

void* APS5_VABI ConcurrentSanitizerWorker(void* argument) {
    auto* context = static_cast<WorkerContext*>(argument);
    for (int iteration = 0; iteration < 200; ++iteration) {
        if (sceKernelIsAddressSanitizerEnabled() != 0) {
            context->failed.store(true);
        }
        if (sceKernelIsThreadSanitizerEnabled() != 0) {
            context->failed.store(true);
        }
        context->completedIterations.fetch_add(1);
    }
    return nullptr;
}

void AuditConcurrentSanitizerQueries() {
    constexpr std::size_t threadCount = 4;
    std::array<Pthread, threadCount> threads{};
    std::array<WorkerContext, threadCount> contexts{};

    for (std::size_t i = 0; i < threadCount; ++i) {
        const int created = scePthreadCreate(&threads[i], nullptr, ConcurrentSanitizerWorker, &contexts[i], "SanitizerWorker");
        Require(created == 0);
    }

    for (std::size_t i = 0; i < threadCount; ++i) {
        void* returnValue = nullptr;
        const int joined = scePthreadJoin(threads[i], &returnValue);
        Require(joined == 0);
        Require(!contexts[i].failed.load());
        Require(contexts[i].completedIterations.load() == 200);
    }
}

template <typename Table>
void RequireZeroedHooks(const Table* table, std::uint64_t expectedSize) {
    Require(table != nullptr);
    Require(table->size == expectedSize);
    const auto* bytes = reinterpret_cast<const unsigned char*>(table) + sizeof(table->size);
    for (std::size_t index = 0; index < sizeof(Table) - sizeof(table->size); ++index) {
        Require(bytes[index] == 0);
    }
}

void AuditSanitizerReplacementTablesConsistency() {
    const auto* mallocTable1 = sceKernelGetSanitizerMallocReplaceExternal();
    RequireZeroedHooks(mallocTable1, 0x70);
    const auto* mallocTable2 = sceKernelGetSanitizerMallocReplaceExternal();
    Require(mallocTable1 == mallocTable2);

    const auto* newTable1 = sceKernelGetSanitizerNewReplaceExternal();
    RequireZeroedHooks(newTable1, 0x68);
    const auto* newTable2 = sceKernelGetSanitizerNewReplaceExternal();
    Require(newTable1 == newTable2);
}

void AuditHeapOperationsUnderDisabledSanitizers() {
    constexpr std::size_t bufferSize = 256;
    auto* memory = static_cast<unsigned char*>(std::malloc(bufferSize));
    Require(memory != nullptr);

    std::memset(memory, 0x5A, bufferSize);
    for (std::size_t i = 0; i < bufferSize; ++i) {
        Require(memory[i] == 0x5A);
    }

    auto* reallocated = static_cast<unsigned char*>(std::realloc(memory, bufferSize * 2));
    Require(reallocated != nullptr);
    for (std::size_t i = 0; i < bufferSize; ++i) {
        Require(reallocated[i] == 0x5A);
    }

    std::free(reallocated);
}

} // namespace

int main() {
    AuditAddressSanitizerDisabledQuery();
    AuditThreadSanitizerDisabledQuery();
    AuditConcurrentSanitizerQueries();
    AuditSanitizerReplacementTablesConsistency();
    AuditHeapOperationsUnderDisabledSanitizers();
    return 0;
}
