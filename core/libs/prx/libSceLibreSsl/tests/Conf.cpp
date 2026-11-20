#include "prx/libSceLibreSsl/libSceLibreSsl.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestErrno.hpp"
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr std::uint64_t IgnoreMissingFile = 0x10;
constexpr std::uint64_t OtherFlags = 0x2f;

constexpr std::uint64_t Error(std::uint64_t library, std::uint64_t reason) {
    return library << 24 | 0xfffull << 12 | reason;
}

constexpr std::uint64_t SystemNoSuchFile = Error(2, 2);
constexpr std::uint64_t SystemNotDirectory = Error(2, 20);
constexpr std::uint64_t BioNoSuchFile = Error(32, 128);
constexpr std::uint64_t BioSystem = Error(32, 2);
constexpr std::uint64_t ConfNoSuchFile = Error(14, 114);
constexpr std::uint64_t ConfSystem = Error(14, 2);
constexpr std::uint64_t ConfMalloc = Error(14, 65);
constexpr std::uint64_t NoSuchEngine = Error(38, 116);

using Codes = std::vector<std::uint64_t>;

const Codes MissingFileErrors{SystemNoSuchFile, BioNoSuchFile, ConfNoSuchFile};
const Codes NotDirectoryErrors{SystemNotDirectory, BioSystem, ConfSystem};

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "failed: %s\n", message);
    std::abort();
}

Codes Drain() {
    Codes codes;
    while (const std::uint64_t code = ERR_get_error_nid_postfix()) codes.push_back(code);
    return codes;
}

int GuestErrno() {
    return *__error_nid_postfix();
}

void QueueEarlierError() {
    Require(ENGINE_by_id_nid_postfix("conf-test") == nullptr, "ENGINE_by_id finds no engine");
}

struct HostDirectory {
    std::filesystem::path path;

    HostDirectory() {
        path = std::filesystem::temp_directory_path() /
            ("anyps5-libressl-conf-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        Require(std::filesystem::create_directory(path), "create the host directory");
        std::ofstream(path / "plain") << "not a directory\n";
    }

    ~HostDirectory() {
        std::error_code ignored;
        std::filesystem::remove_all(path, ignored);
    }

    std::string Plain() const {
        return (path / "plain").string();
    }

    std::string BelowPlain() const {
        return (path / "plain" / "inside").string();
    }
};

void MissingFile(const HostDirectory& host) {
    AddPathAlias_nid_no_patch("/conf-test", host.path.string().c_str());
    constexpr const char* missing = "/conf-test/missing.cnf";

    ERR_clear_error_nid_postfix();
    Require(CONF_modules_load_file_nid_postfix(missing, nullptr, 0) == 0, "a missing file fails without CONF_MFLAGS_IGNORE_MISSING_FILE");
    Require(GuestErrno() == 2, "fopen's ENOENT is left in errno");
    Require(Drain() == MissingFileErrors, "a missing file queues the SYS and BIO errors of BIO_new_file and then CONF_R_NO_SUCH_FILE");

    QueueEarlierError();
    Require(CONF_modules_load_file_nid_postfix(missing, nullptr, OtherFlags) == 0, "only CONF_MFLAGS_IGNORE_MISSING_FILE ignores a missing file");
    Codes expected{NoSuchEngine};
    expected.insert(expected.end(), MissingFileErrors.begin(), MissingFileErrors.end());
    Require(Drain() == expected, "the errors follow the ones already queued");

    QueueEarlierError();
    Require(CONF_modules_load_file_nid_postfix(missing, "application", IgnoreMissingFile) == 1, "an ignored missing file succeeds");
    Require(Drain().empty(), "an ignored missing file clears the whole error queue");

    RemovePathAlias_nid_no_patch("/conf-test");
}

void PathThroughAFile(const HostDirectory& host) {
    AddPathAlias_nid_no_patch("/conf-test", host.Plain().c_str());
    ERR_clear_error_nid_postfix();
    Require(CONF_modules_load_file_nid_postfix("/conf-test/openssl.cnf", nullptr, IgnoreMissingFile) == 0,
        "a path below a regular file is not a missing file");
    Require(GuestErrno() == 20, "fopen's ENOTDIR is left in errno");
    Require(Drain() == NotDirectoryErrors, "another fopen error queues ERR_R_SYS_LIB for BIO and CONF");
    RemovePathAlias_nid_no_patch("/conf-test");
}

void DefaultFile(const HostDirectory& host) {
    AddPathAlias_nid_no_patch("/etc/ssl", host.path.string().c_str());
    ERR_clear_error_nid_postfix();
    Require(CONF_modules_load_file_nid_postfix(nullptr, nullptr, 0) == 0, "a missing default file fails");
    Require(Drain() == MissingFileErrors, "a missing default file queues the missing-file errors");
    QueueEarlierError();
    Require(CONF_modules_load_file_nid_postfix(nullptr, nullptr, IgnoreMissingFile) == 1, "an ignored missing default file succeeds");
    Require(Drain().empty(), "an ignored missing default file clears the error queue");
    RemovePathAlias_nid_no_patch("/etc/ssl");

    AddPathAlias_nid_no_patch("/etc/ssl/openssl.cnf", host.BelowPlain().c_str());
    Require(CONF_modules_load_file_nid_postfix(nullptr, nullptr, IgnoreMissingFile) == 0, "the default file is /etc/ssl/openssl.cnf");
    Require(Drain() == NotDirectoryErrors, "the default path below a regular file is not a missing file");
    RemovePathAlias_nid_no_patch("/etc/ssl/openssl.cnf");
}

std::vector<std::size_t> allocated;
int frees = 0;
int failAt = 0;
int calls = 0;
int throwAt = 0;
unsigned throwFrees = 0;
std::array<void*, 4> allocationPointers{};
std::array<void*, 4> releasePointers{};

struct AllocationFailure {};
struct FreeFailure { int ordinal; };

void ResetAllocations(int failing) {
    allocated.clear();
    frees = calls = 0;
    failAt = failing;
    throwAt = 0;
    throwFrees = 0;
    allocationPointers = {};
    releasePointers = {};
}

void* APS5_VABI Allocate_nid_no_patch(std::size_t size) {
    ++calls;
    if (calls == throwAt) throw AllocationFailure{};
    if (calls == failAt) return nullptr;
    void* const pointer = std::malloc(size);
    if (pointer) {
        Require(allocated.size() < allocationPointers.size(), "allocation ledger capacity");
        allocationPointers[allocated.size()] = pointer;
        allocated.push_back(size);
    }
    return pointer;
}

void APS5_VABI Free_nid_no_patch(void* pointer) {
    if (!pointer) return;
    bool found = false;
    for (void* allocation : allocationPointers) found = found || allocation == pointer;
    Require(found, "free uses the matching application allocation");
    for (void* release : releasePointers) Require(release != pointer, "each pointer is freed at most once");
    Require(static_cast<std::size_t>(frees) < releasePointers.size(), "release ledger capacity");
    releasePointers[frees++] = pointer;
    std::free(pointer);
    if (throwFrees & (1u << (frees - 1))) throw FreeFailure{frees};
}

void* APS5_VABI Calloc_nid_no_patch(std::size_t, std::size_t) { throw std::runtime_error("unexpected calloc"); }
void* APS5_VABI Reallocate_nid_no_patch(void*, std::size_t) { throw std::runtime_error("unexpected realloc"); }
void* APS5_VABI Align_nid_no_patch(std::size_t, std::size_t) { throw std::runtime_error("unexpected aligned allocation"); }
void* APS5_VABI Realign_nid_no_patch(void*, std::size_t, std::size_t) { throw std::runtime_error("unexpected aligned reallocation"); }
int APS5_VABI PosixAlign_nid_no_patch(void**, std::size_t, std::size_t) { throw std::runtime_error("unexpected posix_memalign"); }

void CheckCleanup(std::size_t count) {
    bool matching = allocated.size() == count && static_cast<std::size_t>(frees) == count;
    for (std::size_t i = 0; i < count; ++i) matching = matching && releasePointers[i] == allocationPointers[count - 1 - i];
    if (!matching) {
        std::fprintf(stderr, "CONF cleanup failed: allocations=%zu releases=%d expected=%zu\n", allocated.size(), frees, count);
        std::abort();
    }
}

void CleanupExceptions(const HostDirectory& host) {
    for (unsigned cleanupThrows : {0u, 1u}) {
        QueueEarlierError();
        ResetAllocations(0);
        throwAt = 2;
        throwFrees = cleanupThrows;
        bool caught = false;
        try {
            CONF_modules_load_file_nid_postfix(nullptr, nullptr, IgnoreMissingFile);
        } catch (const AllocationFailure&) {
            caught = true;
        } catch (...) {
            Require(false, "default-path allocation exception keeps precedence");
        }
        throwAt = 0;
        throwFrees = 0;
        Require(caught && calls == 2, "the second allocation callback throws before allocating the default path");
        CheckCleanup(1);
        Require(allocated == std::vector<std::size_t>{32}, "only the CONF allocation completed");
        Require(Drain() == Codes{NoSuchEngine}, "allocation exceptions preserve the error queue");
    }

    for (bool existing : {false, true}) {
        if (existing) std::ofstream(host.path / "openssl.cnf") << "existing configuration\n";
        for (bool useDefault : {false, true}) {
            const unsigned limit = useDefault ? 4 : 2;
            for (unsigned mask = 0; mask < limit; ++mask) {
                QueueEarlierError();
                ResetAllocations(0);
                throwFrees = mask;
                bool returned = false;
                bool unsupported = false;
                int freeFailure = 0;
                try {
                    returned = CONF_modules_load_file_nid_postfix(useDefault ? nullptr :
                        (existing ? "/etc/ssl/plain" : "/etc/ssl/missing.cnf"), nullptr, IgnoreMissingFile) == 1;
                } catch (const FreeFailure& error) {
                    freeFailure = error.ordinal;
                } catch (const std::exception&) {
                    unsupported = true;
                } catch (...) {
                    Require(false, "unexpected CONF cleanup exception");
                }
                throwFrees = 0;
                CheckCleanup(useDefault ? 2 : 1);
                Require(calls == (useDefault ? 2 : 1), "cleanup failure reaches the expected allocation stage");
                Require(mask ? freeFailure == ((mask & 1) ? 1 : 2) : freeFailure == 0,
                    "the first free exception is propagated without retry");
                Require(unsupported == (existing && mask == 0), "existing files still throw after successful cleanup");
                Require(returned == (!existing && mask == 0), "missing-file success is preserved");
                Require(Drain() == (existing ? Codes{NoSuchEngine} : Codes{}), "cleanup preserves the existing error-queue policy");
            }
        }
    }
    Require(std::filesystem::remove(host.path / "openssl.cnf"), "remove the existing default configuration");
}

void Allocator(const HostDirectory& host) {
    std::array<void*, 10> api{
        reinterpret_cast<void*>(Allocate_nid_no_patch), reinterpret_cast<void*>(Free_nid_no_patch), reinterpret_cast<void*>(Calloc_nid_no_patch),
        reinterpret_cast<void*>(Reallocate_nid_no_patch), reinterpret_cast<void*>(Align_nid_no_patch), reinterpret_cast<void*>(Realign_nid_no_patch),
        reinterpret_cast<void*>(PosixAlign_nid_no_patch)};
    ApplicationHeapRegister_nid_no_patch(api.data());
    AddPathAlias_nid_no_patch("/etc/ssl", host.path.string().c_str());

    ERR_clear_error_nid_postfix();
    ResetAllocations(0);
    Require(CONF_modules_load_file_nid_postfix("/etc/ssl/missing.cnf", nullptr, IgnoreMissingFile) == 1, "a named missing file is ignored");
    Require(allocated == std::vector<std::size_t>{32}, "a named file allocates only the CONF object");
    Require(frees == 1, "the CONF object is freed");

    ResetAllocations(0);
    Require(CONF_modules_load_file_nid_postfix(nullptr, nullptr, IgnoreMissingFile) == 1, "the missing default file is ignored");
    Require(allocated == std::vector<std::size_t>{32, sizeof("/etc/ssl/openssl.cnf")}, "the CONF object, then the default path");
    Require(frees == 2, "the default path and the CONF object are freed");

    for (const char* filename : {"/etc/ssl/missing.cnf", static_cast<const char*>(nullptr)}) {
        ERR_clear_error_nid_postfix();
        ResetAllocations(1);
        Require(CONF_modules_load_file_nid_postfix(filename, nullptr, IgnoreMissingFile) == 0, "a failed CONF allocation fails");
        Require(Drain() == Codes{ConfMalloc}, "a failed CONF allocation queues only the CONF malloc failure");
        Require(allocated.empty() && frees == 0, "nothing else is allocated or freed");
    }

    ERR_clear_error_nid_postfix();
    ResetAllocations(2);
    Require(CONF_modules_load_file_nid_postfix(nullptr, nullptr, IgnoreMissingFile) == 0, "a failed default path allocation fails");
    Require(Drain().empty(), "a failed default path allocation queues no error");
    Require(GuestErrno() == 12, "asprintf's ENOMEM is left in errno");
    Require(allocated == std::vector<std::size_t>{32} && frees == 1, "the CONF object is freed");

    CleanupExceptions(host);
    ResetAllocations(0);
    RemovePathAlias_nid_no_patch("/etc/ssl");
}

} // namespace

int main(int argc, char**) {
    const HostDirectory host;
    if (argc > 1) {
        Allocator(host);
        return 0;
    }
    std::array<void*, 10> api{};
    ApplicationHeapRegister_nid_no_patch(api.data());
    MissingFile(host);
    PathThroughAFile(host);
    DefaultFile(host);
}
