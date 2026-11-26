#include "prx/libSceLibreSsl/libSceLibreSsl.hpp"
#include "prx/libSceLibreSsl/include/ObjectTable.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestErrno.hpp"
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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
constexpr std::uint64_t SystemNameTooLong = Error(2, 63);
constexpr std::uint64_t BioNoSuchFile = Error(32, 128);
constexpr std::uint64_t BioSystem = Error(32, 2);
constexpr std::uint64_t ConfNoSuchFile = Error(14, 114);
constexpr std::uint64_t ConfSystem = Error(14, 2);
constexpr std::uint64_t ConfMalloc = Error(14, 65);
constexpr std::uint64_t NoSuchEngine = Error(38, 116);
constexpr std::uint64_t DsoErrorLoadingDso = Error(37, 108);
constexpr std::uint64_t ConfErrorLoadingDso = Error(14, 110);
constexpr std::uint64_t ConfUnknownModule = Error(14, 113);
constexpr std::uint64_t ConfMissingEqualSign = Error(14, 101);
constexpr std::uint64_t ConfMissingCloseSquareBracket = Error(14, 100);
constexpr std::uint64_t ConfVariableHasNoValue = Error(14, 104);
constexpr std::uint64_t ConfModuleInitialization = Error(14, 109);
constexpr std::uint64_t Asn1InvalidDigit = Error(13, 130);
constexpr std::uint64_t Asn1AddingObject = Error(13, 171);
constexpr std::uint64_t EngineConfigurationError = Error(38, 102);

using Codes = std::vector<std::uint64_t>;

const Codes MissingFileErrors{SystemNoSuchFile, BioNoSuchFile, ConfNoSuchFile};
const Codes NotDirectoryErrors{SystemNotDirectory, BioSystem, ConfSystem};
const Codes NameTooLongErrors{SystemNameTooLong, BioSystem, ConfSystem};

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "failed: %s\n", message);
    std::abort();
}

bool Equal(const char* actual, const char* expected) {
    return actual && std::strcmp(actual, expected) == 0;
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
        if (existing) {
            std::ofstream(host.path / "openssl.cnf") << "# existing configuration\n";
            std::ofstream(host.path / "plain") << "# existing configuration\n";
        }
        for (bool useDefault : {false, true}) {
            const unsigned limit = useDefault ? 4 : 2;
            for (unsigned mask = 0; mask < limit; ++mask) {
                QueueEarlierError();
                ResetAllocations(0);
                throwFrees = mask;
                bool returned = false;
                int freeFailure = 0;
                try {
                    returned = CONF_modules_load_file_nid_postfix(useDefault ? nullptr :
                        (existing ? "/etc/ssl/plain" : "/etc/ssl/missing.cnf"), nullptr, IgnoreMissingFile) == 1;
                } catch (const FreeFailure& error) {
                    freeFailure = error.ordinal;
                } catch (...) {
                    Require(false, "unexpected CONF cleanup exception");
                }
                throwFrees = 0;
                CheckCleanup(useDefault ? 2 : 1);
                Require(calls == (useDefault ? 2 : 1), "cleanup failure reaches the expected allocation stage");
                Require(mask ? freeFailure == ((mask & 1) ? 1 : 2) : freeFailure == 0,
                    "the first free exception is propagated without retry");
                Require(returned == (mask == 0), "an existing configuration file loads after successful cleanup");
                Require(Drain().empty(), "a configuration without an appname clears the error queue");
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

void NameTooLong() {
    ERR_clear_error_nid_postfix();
    const std::string longPath(1024, 'a');
    Require(CONF_modules_load_file_nid_postfix(longPath.c_str(), nullptr, IgnoreMissingFile) == 0,
        "CONF_modules_load_file fails on ENAMETOOLONG even with IgnoreMissingFile");
    Require(GuestErrno() == 63, "fopen leaves ENAMETOOLONG in guest errno");
    Require(Drain() == NameTooLongErrors,
        "CONF_modules_load_file queues guest ENAMETOOLONG and system errors");
}

std::string WriteConfig(const HostDirectory& host, const char* name, const char* text) {
    std::ofstream(host.path / name) << text;
    return std::string("/conf-test/") + name;
}

void Modules(const HostDirectory& host) {
    constexpr std::uint64_t IgnoreErrors = 0x1;
    AddPathAlias_nid_no_patch("/conf-test", host.path.string().c_str());
    OPENSSL_load_builtin_modules_nid_postfix();
    OPENSSL_load_builtin_modules_nid_postfix();

    const std::string oidConfig = WriteConfig(host, "oids.cnf",
        "openssl_conf = openssl_init\n\n[openssl_init]\noid_section = new_oids\n\n"
        "[new_oids]\ntestoid1 = 1.2.3.4.5\ntestoid2 = Long Test OID, 1.2.3.4.6\n");
    ERR_clear_error_nid_postfix();
    Require(CONF_modules_load_file_nid_postfix(oidConfig.c_str(), nullptr, 0) == 1, "oid_section creates objects");
    Require(Drain().empty(), "oid_section queues no error");
    Require(OBJ_sn2nid_nid_postfix("testoid1") == 956, "the first OID takes NID 956");
    Require(Equal(OBJ_nid2sn_nid_postfix(956), "testoid1"), "the short name is the entry name");
    Require(Equal(LibreSslAddedObjectName_nid_no_patch(957, true), "Long Test OID"), "the comma prefix is the long name");
    CONF_modules_free_nid_postfix();
    CONF_modules_free_nid_postfix();
    Require(OBJ_sn2nid_nid_postfix("testoid1") == 0, "CONF_modules_free removes added objects");

    OPENSSL_load_builtin_modules_nid_postfix();
    const std::string custom = WriteConfig(host, "custom.cnf",
        "test_app = test_init\n[test_init]\noid_section = test_oids\n[test_oids]\ntestoid3 = 1.2.3.4.7\n");
    ERR_clear_error_nid_postfix();
    Require(CONF_modules_load_file_nid_postfix(custom.c_str(), "test_app", 0) == 1, "a custom appname selects its section");
    Require(Drain().empty(), "a custom appname queues no error");
    Require(OBJ_sn2nid_nid_postfix("testoid3") == 958, "the custom OID takes NID 958");
    CONF_modules_free_nid_postfix();

    OPENSSL_load_builtin_modules_nid_postfix();
    ERR_clear_error_nid_postfix();
    Require(CONF_modules_load_file_nid_postfix(custom.c_str(), "absent", 0) == 1, "an absent appname succeeds");
    Require(Drain().empty(), "an absent appname queues no error");

    const std::string unknown = WriteConfig(host, "unknown.cnf", "openssl_conf = test_init\n[test_init]\nno_such_module = value\n");
    for (std::uint64_t flags : {std::uint64_t{0}, IgnoreErrors}) {
        ERR_clear_error_nid_postfix();
        Require(CONF_modules_load_file_nid_postfix(unknown.c_str(), nullptr, flags) == (flags == 0 ? -1 : 1),
            "an unknown module follows CONF_MFLAGS_IGNORE_ERRORS");
        Require(Drain() == Codes{DsoErrorLoadingDso, ConfErrorLoadingDso, ConfUnknownModule},
            "an unknown module queues the DSO and CONF errors");
    }

    const std::string badSyntax = WriteConfig(host, "syntax.cnf", "not an assignment\n");
    ERR_clear_error_nid_postfix();
    Require(CONF_modules_load_file_nid_postfix(badSyntax.c_str(), nullptr, 0) == 0, "a syntax error fails parsing");
    Require(Drain() == Codes{ConfMissingEqualSign}, "a syntax error queues CONF_R_MISSING_EQUAL_SIGN");

    const std::string badOid = WriteConfig(host, "badoid.cnf",
        "openssl_conf = test_init\n[test_init]\noid_section = bad_oids\n[bad_oids]\ntestbad = 1.2.x.4\n");
    ERR_clear_error_nid_postfix();
    Require(CONF_modules_load_file_nid_postfix(badOid.c_str(), nullptr, 0) == -1, "a bad OID fails module initialization");
    Require(Drain() == Codes{Asn1InvalidDigit, Asn1AddingObject, ConfModuleInitialization},
        "a bad OID queues ASN.1 and CONF errors");

    const std::string badEngine = WriteConfig(host, "badeng.cnf",
        "openssl_conf = test_init\n[test_init]\nengines = eng_section\n[eng_section]\ntestengine = eng_cfg\n[eng_cfg]\ninit = 0\n");
    ERR_clear_error_nid_postfix();
    Require(CONF_modules_load_file_nid_postfix(badEngine.c_str(), nullptr, 0) == -1, "a missing engine fails module initialization");
    Require(Drain() == Codes{NoSuchEngine, EngineConfigurationError, ConfModuleInitialization},
        "a missing engine queues ENGINE and CONF errors");

    const std::string emptyEngine = WriteConfig(host, "emptyeng.cnf",
        "openssl_conf = test_init\n[test_init]\nengines = eng_section\n[eng_section]\n");
    ERR_clear_error_nid_postfix();
    Require(CONF_modules_load_file_nid_postfix(emptyEngine.c_str(), nullptr, 0) == 1, "an empty engines section succeeds");
    Require(Drain().empty(), "an empty engines section queues no error");

    const std::string empty = WriteConfig(host, "empty.cnf", "");
    ERR_clear_error_nid_postfix();
    Require(CONF_modules_load_file_nid_postfix(empty.c_str(), nullptr, 0) == 1, "an empty configuration file succeeds");
    Require(Drain().empty(), "an empty configuration file queues no error");

    const std::string variable = WriteConfig(host, "variable.cnf",
        "openssl_conf = openssl_init\nbase_oid = 1.2.3.4.8\n[openssl_init]\noid_section = new_oids\n\n"
        "[new_oids]\ntestvar = ${default::base_oid}\n");
    ERR_clear_error_nid_postfix();
    Require(CONF_modules_load_file_nid_postfix(variable.c_str(), nullptr, 0) == 1, "variable expansion loads");
    Require(Drain().empty(), "variable expansion queues no error");
    Require(OBJ_sn2nid_nid_postfix("testvar") == 959, "the expanded OID is created");

    CONF_modules_free_nid_postfix();
    OPENSSL_load_builtin_modules_nid_postfix();
    ERR_clear_error_nid_postfix();
    Require(CONF_modules_load_file_nid_postfix(variable.c_str(), nullptr, 0) == 1, "a reload after CONF_modules_free succeeds");
    Require(OBJ_sn2nid_nid_postfix("testvar") == 960, "the reload creates the OID again");
    Require(Drain().empty(), "a reload queues no error");
    CONF_modules_free_nid_postfix();
    RemovePathAlias_nid_no_patch("/conf-test");
}

void ParserVectors(const HostDirectory& host) {
    struct Vector {
        const char* file;
        const char* content;
        const char* section;
        const char* name;
        int loaded;
        long line;
        const char* value;
        Codes errors;
    };
    const std::vector<Vector> vectors{
        {"duplicate.cnf", "openssl_conf=old\nopenssl_conf=new\n", "default", "openssl_conf", 1, 0, "new", {}},
        {"envmissing.cnf", "oid=$ENV::CONF_REVIEW_ENV\n", "default", "oid", 0, 1, nullptr, {ConfVariableHasNoValue}},
        {"envsection.cnf", "[ENV]\nCONF_REVIEW_ENV=1.2.3.4.66\n[default]\noid=$ENV::CONF_REVIEW_ENV\n", "default", "oid", 1, 0, "1.2.3.4.66", {}},
        {"envfallback.cnf", "CONF_REVIEW_ENV=1.2.3.4.77\noid=$ENV::CONF_REVIEW_ENV\n", "default", "oid", 1, 0, "1.2.3.4.77", {}},
        {"quotes.cnf", "value=\"one\"\"two\"\n", "default", "value", 1, 0, "onetwo", {}},
        {"escapedquote.cnf", "value='one\\'two'\n", "default", "value", 1, 0, "one'two", {}},
        {"quotedsection.cnf", "[\"my section\"]\nvalue=hello\n", "my section", "value", 0, 1, nullptr, {ConfMissingCloseSquareBracket}},
        {"spacesection.cnf", "[my section]\nvalue=hello\n", "my section", "value", 1, 0, "hello", {}},
        {"emptysection.cnf", "[]\nvalue=hello\n", "", "value", 1, 0, "hello", {}},
        {"dollarsection.cnf", "[my$section]\nvalue=hello\n", "my$section", "value", 0, 1, nullptr, {ConfMissingCloseSquareBracket}},
        {"escapename.cnf", "a\\ b=value\n", "default", "a\\ b", 1, 0, "value", {}},
        {"continuation.cnf", "value=one\\\ntwo\n", "default", "value", 1, 0, "onetwo", {}},
        {"badcontinuation.cnf", "value=one\\\n$missing\n", "default", "value", 0, 2, nullptr, {ConfVariableHasNoValue}},
        {"comment.cnf", "value=\"one#two\" # ignored\n", "default", "value", 1, 0, "one#two", {}},
        {"fallback.cnf", "value=hello\n[other]\n", "other", "value", 1, 0, "hello", {}},
    };
    Require(setenv_nid_postfix("CONF_REVIEW_ENV", "1.2.3.4.55", 1) == 0, "set guest environment");
    AddPathAlias_nid_no_patch("/conf-test", host.path.string().c_str());
    for (const auto& vector : vectors) {
        const std::string path = WriteConfig(host, vector.file, vector.content);
        void* conf = NCONF_new_nid_no_patch(nullptr);
        long line = 0;
        ERR_clear_error_nid_postfix();
        const int loaded = NCONF_load_nid_no_patch(conf, path.c_str(), &line);
        Require(loaded == vector.loaded && line == vector.line, vector.file);
        if (loaded) Require(Equal(NCONF_get_string_nid_no_patch(conf, vector.section, vector.name), vector.value), vector.file);
        Require(Drain() == vector.errors, vector.file);
        NCONF_free_nid_no_patch(conf);
    }
    OPENSSL_load_builtin_modules_nid_postfix();
    const std::string path = WriteConfig(host, "moduleduplicate.cnf",
        "openssl_conf=first\nopenssl_conf=second\n[first]\nno_such_module=bad\n"
        "[second]\noid_section=objects\n[objects]\ntestoid_review=1.2.3.4.199\n");
    ERR_clear_error_nid_postfix();
    Require(CONF_modules_load_file_nid_postfix(path.c_str(), nullptr, 0) == 1, "later module section wins");
    Require(OBJ_sn2nid_nid_postfix("testoid_review") != 0 && Drain().empty(), "later module section creates its OID");
    CONF_modules_free_nid_postfix();
    RemovePathAlias_nid_no_patch("/conf-test");
    Require(unsetenv_nid_postfix("CONF_REVIEW_ENV") == 0, "clear guest environment");
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
    NameTooLong();
    Modules(host);
    ParserVectors(host);
}
