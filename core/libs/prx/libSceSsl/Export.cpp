#include "SslTypes.hpp"
#include "prx/libc/include/General.hpp"

#include <mbedtls/asn1.h>
#include <mbedtls/base64.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/md.h>
#include <mbedtls/oid.h>
#include <mbedtls/pem.h>
#include <mbedtls/pk.h>
#include <mbedtls/platform.h>
#include <mbedtls/platform_util.h>
#include <mbedtls/x509_crt.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

constexpr int ErrorNotFound = static_cast<int>(0x8095f004);
constexpr int ErrorInvalidArg = static_cast<int>(0x8095177a);
constexpr int ErrorInvalidId = static_cast<int>(0x8095f006);

constexpr std::int64_t TicksPerSecond = 1000000;
constexpr std::int64_t TicksPerMinute = 60 * TicksPerSecond;
constexpr std::int64_t TicksPerHour = 60 * TicksPerMinute;
constexpr std::int64_t TicksPerDay = 24 * TicksPerHour;
constexpr std::int64_t UnixEpochDays = 719162;

std::int64_t DaysFromCivil(std::int64_t year, unsigned month, unsigned day) {
    year -= month <= 2;
    const std::int64_t era = (year >= 0 ? year : year - 399) / 400;
    const auto yearOfEra = static_cast<unsigned>(year - era * 400);
    const unsigned dayOfYear = (153 * (month > 2 ? month - 3 : month + 9) + 2) / 5 + day - 1;
    const unsigned dayOfEra = yearOfEra * 365 + yearOfEra / 4 - yearOfEra / 100 + dayOfYear;
    return era * 146097 + static_cast<std::int64_t>(dayOfEra) - 719468 + UnixEpochDays;
}

std::uint64_t TimeToTick(const mbedtls_x509_time& time) {
    return static_cast<std::uint64_t>(DaysFromCivil(time.year, time.mon, time.day)) * TicksPerDay
        + time.hour * TicksPerHour + time.min * TicksPerMinute + time.sec * TicksPerSecond;
}

struct Pool {
    std::size_t size, current = 0, peak = 0;
    bool exhausted = false;
};

struct alignas(std::max_align_t) Allocation {
    Pool* pool;
    std::size_t size;
};

thread_local Pool* currentPool = nullptr;

void* PoolCalloc(std::size_t count, std::size_t size) noexcept {
    if (!currentPool) return nullptr;
    if (size && count > std::numeric_limits<std::size_t>::max() / size) {
        currentPool->exhausted = true;
        return nullptr;
    }
    const auto bytes = count * size;
    if (bytes > currentPool->size - currentPool->current || bytes > std::numeric_limits<std::size_t>::max() - sizeof(Allocation)) {
        currentPool->exhausted = true;
        return nullptr;
    }
    auto* allocation = static_cast<Allocation*>(std::calloc(1, sizeof(Allocation) + bytes));
    if (!allocation) {
        currentPool->exhausted = true;
        return nullptr;
    }
    allocation->pool = currentPool;
    allocation->size = bytes;
    currentPool->current += bytes;
    currentPool->peak = std::max(currentPool->peak, currentPool->current);
    return allocation + 1;
}

void PoolFree(void* pointer) noexcept {
    if (!pointer) return;
    auto* allocation = static_cast<Allocation*>(pointer) - 1;
    allocation->pool->current -= allocation->size;
    mbedtls_platform_zeroize(pointer, allocation->size);
    std::free(allocation);
}

struct PoolScope {
    Pool* previous;
    explicit PoolScope(Pool& pool) : previous(std::exchange(currentPool, &pool)) { pool.exhausted = false; }
    ~PoolScope() { currentPool = previous; }
};

struct PoolDeleter {
    void operator()(std::uint8_t* bytes) const { PoolFree(bytes); }
};

using Block = std::unique_ptr<std::uint8_t, PoolDeleter>;

Block Allocate(std::size_t size) {
    Block block(static_cast<std::uint8_t*>(PoolCalloc(1, std::max<std::size_t>(1, size))));
    if (!block) throw std::bad_alloc();
    return block;
}

std::size_t Add(std::size_t left, std::size_t right) {
    if (right > std::numeric_limits<std::size_t>::max() - left) throw std::length_error("SSL allocation size overflow");
    return left + right;
}

void CheckBackend(int error, const char* message) {
    if (error == 0) return;
    if (currentPool && currentPool->exhausted) throw std::bad_alloc();
    throw std::invalid_argument(message);
}

struct CertificateChain;

struct CertificateHandle {
    std::weak_ptr<CertificateChain> owner;
    const mbedtls_x509_crt* certificate;
};

struct CertificateChain {
    mbedtls_x509_crt root;
    std::vector<CertificateHandle*> handles;
    CertificateChain() { mbedtls_x509_crt_init(&root); }
    ~CertificateChain() { mbedtls_x509_crt_free(&root); }
};

struct PrivateKey {
    mbedtls_pk_context key;
    PrivateKey() { mbedtls_pk_init(&key); }
    ~PrivateKey() { mbedtls_pk_free(&key); }
};

struct Random {
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context drbg;
    Random() {
        mbedtls_entropy_init(&entropy);
        mbedtls_ctr_drbg_init(&drbg);
    }
    ~Random() {
        mbedtls_ctr_drbg_free(&drbg);
        mbedtls_entropy_free(&entropy);
    }
    void Seed() {
        CheckBackend(mbedtls_ctr_drbg_seed(&drbg, mbedtls_entropy_func, &entropy, nullptr, 0), "SSL random generator initialization failed");
    }
};

struct Identity {
    std::shared_ptr<CertificateChain> chain;
    PrivateKey key;
};

struct Snapshot {
    Block data;
    std::vector<std::shared_ptr<CertificateChain>> certificates;
    std::size_t count;
};

struct CertNameEntry {
    std::string oid;
    std::string value;
};

struct CertName {
    std::vector<CertNameEntry> entries;
};

struct Context {
    Pool pool;
    std::vector<std::shared_ptr<CertificateChain>> authorities;
    std::shared_ptr<Identity> identity;
    std::vector<std::unique_ptr<CertificateHandle>> certificateHandles;
    std::unordered_map<void*, Snapshot> caLists;
    std::unordered_map<void*, Snapshot> caCerts;
    std::unordered_map<void*, std::unique_ptr<CertName>> certNames;
    explicit Context(std::size_t size) : pool{size} {}
};

using SslVerifyCallback = int (APS5_VABI *)(int, unsigned int, void**, int, void*);

constexpr int SslVersionNone = 0;
constexpr int SslVersionTls12 = 5;

struct SslConnection {
    int context;
    int socket;
    std::string host;
    SslVerifyCallback verifyCallback;
    void* verifyArgument;
    std::uint32_t verifyOptions = 0;
    int minimumVersion = SslVersionNone;
    std::string alpn;
};

struct Runtime {
    std::mutex mutex;
    std::unordered_map<int, std::unique_ptr<Context>> contexts;
    std::unordered_map<int, SslConnection> connections;
    unsigned nextHandle = 1;
    Runtime() {
        if (mbedtls_platform_set_calloc_free(PoolCalloc, PoolFree) != 0)
            throw std::runtime_error("SSL allocator initialization failed");
    }
};

Runtime& State() {
    static auto* runtime = new Runtime;
    return *runtime;
}

Context& GetContext(int handle) {
    const auto found = State().contexts.find(handle);
    if (found == State().contexts.end()) throw std::invalid_argument("Invalid SSL context");
    return *found->second;
}

SslConnection& RequireConnection(const char* function, int sslConnectionId) {
    const auto connection = State().connections.find(sslConnectionId);
    if (connection == State().connections.end()) throw std::invalid_argument(std::string(function) + ": unknown connection");
    if (!State().contexts.contains(connection->second.context)) {
        throw std::invalid_argument(std::string(function) + ": the connection's context was terminated");
    }
    return connection->second;
}

struct Input {
    Block bytes;
    std::size_t size;
};

Input CopyInput(const SceSsl::Data& data) {
    if (!data.ptr || !data.size) throw std::invalid_argument("Empty SSL certificate/key data");
    auto bytes = Allocate(Add(data.size, 1));
    std::memcpy(bytes.get(), data.ptr, data.size);
    auto size = data.size;
    std::size_t first = 0;
    while (first < size && (bytes.get()[first] == ' ' || bytes.get()[first] == '\r' || bytes.get()[first] == '\n' || bytes.get()[first] == '\t')) ++first;
    const bool pem = first < size && bytes.get()[first] == '-';
    if (pem) {
        if (bytes.get()[size - 1] != 0) ++size;
        if (std::memchr(bytes.get(), 0, size - 1)) throw std::invalid_argument("Embedded null in SSL PEM data");
    } else {
        auto* position = bytes.get();
        std::size_t length = 0;
        CheckBackend(mbedtls_asn1_get_tag(&position, bytes.get() + size, &length, MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE),
            "Invalid SSL DER sequence");
        if (length != static_cast<std::size_t>(bytes.get() + size - position)) throw std::invalid_argument("Trailing SSL DER data");
    }
    return {std::move(bytes), size};
}

std::shared_ptr<CertificateChain> ParseCertificate(const SceSsl::Data& data) {
    auto chain = std::make_shared<CertificateChain>();
    const auto input = CopyInput(data);
    CheckBackend(mbedtls_x509_crt_parse(&chain->root, input.bytes.get(), input.size), "Invalid SSL certificate chain");
    return chain;
}

std::size_t CertificateCount(const Context& context) {
    std::size_t count = 0;
    for (const auto& chain : context.authorities) count = Add(count, chain->handles.size());
    return count;
}

const mbedtls_x509_crt* RequireCertificate(const char* function, int sslCtxId, void* cert) {
    if (!cert) throw std::invalid_argument(std::string(function) + ": null certificate");
    auto& context = GetContext(sslCtxId);
    for (const auto& handle : context.certificateHandles) {
        if (handle.get() == cert) {
            if (handle->owner.expired() || !handle->certificate) {
                throw std::invalid_argument(std::string(function) + ": expired certificate");
            }
            return handle->certificate;
        }
    }
    throw std::invalid_argument(std::string(function) + ": unknown certificate");
}

void* ExtractCertName(const char* function, int sslCtxId, void* sslCert, bool subject) {
    std::lock_guard lock(State().mutex);
    auto& context = GetContext(sslCtxId);
    const auto* crt = RequireCertificate(function, sslCtxId, sslCert);
    const auto* name = subject ? &crt->subject : &crt->issuer;
    auto certName = std::make_unique<CertName>();
    for (const auto* node = name; node; node = node->next) {
        if (!node->oid.p || node->oid.len == 0) continue;
        std::string oid;
        const char* shortName = nullptr;
        if (mbedtls_oid_get_attr_short_name(&node->oid, &shortName) == 0 && shortName) {
            oid = shortName;
        } else {
            char numeric[64] = {};
            if (mbedtls_oid_get_numeric_string(numeric, sizeof(numeric), &node->oid) > 0) {
                oid = numeric;
            }
        }
        std::string value;
        if (node->val.p && node->val.len > 0) {
            value.assign(reinterpret_cast<const char*>(node->val.p), node->val.len);
        }
        certName->entries.push_back({std::move(oid), std::move(value)});
    }
    auto* handle = certName.get();
    context.certNames.emplace(handle, std::move(certName));
    return handle;
}

}

extern "C" {

int APS5_VABI sceSslInit_nid_postfix(std::uint64_t poolSize) {
    auto& state = State();
    std::lock_guard lock(state.mutex);
    if (state.nextHandle > static_cast<unsigned>(std::numeric_limits<int>::max())) throw std::overflow_error("SSL context handles exhausted");
    const auto handle = static_cast<int>(state.nextHandle);
    state.contexts.emplace(handle, std::make_unique<Context>(poolSize));
    ++state.nextHandle;
    return handle;
}

int APS5_VABI sceSslTerm_nid_postfix(int sslCtxId) {
    auto& state = State();
    std::lock_guard lock(state.mutex);
    state.contexts.erase(sslCtxId);
    return 0;
}

int APS5_VABI sceSslGetMemoryPoolStats(int sslCtxId, SceSsl::MemoryPoolStats* stats) {
    if (!stats) return static_cast<int>(0x8095F007);
    auto& state = State();
    std::lock_guard lock(state.mutex);
    const auto found = state.contexts.find(sslCtxId);
    if (found == state.contexts.end()) {
        std::memset(stats, 0, sizeof(*stats));
        return static_cast<int>(0x8095F006);
    }
    const auto& pool = found->second->pool;
    *stats = {pool.size, pool.peak, pool.current, 0};
    return 0;
}

int APS5_VABI sceSslLoadCert(int sslCtxId, int caCertNum, SceSsl::Data** caList, SceSsl::Data* cert, SceSsl::Data* privateKey) {
    if (caCertNum < 0 || (caCertNum && !caList) || (!!cert != !!privateKey))
        throw std::invalid_argument("Invalid SSL certificate load arguments");
    auto& state = State();
    std::lock_guard lock(state.mutex);
    auto& context = GetContext(sslCtxId);
    PoolScope scope(context.pool);
    std::vector<std::shared_ptr<CertificateChain>> authorities;
    std::vector<std::unique_ptr<CertificateHandle>> handles;
    for (int index = 0; index < caCertNum; ++index) {
        if (!caList[index]) throw std::invalid_argument("Missing SSL certificate descriptor");
        auto chain = ParseCertificate(*caList[index]);
        for (auto* entry = &chain->root; entry; entry = entry->next) {
            auto handle = std::make_unique<CertificateHandle>(CertificateHandle{chain, entry});
            chain->handles.push_back(handle.get());
            handles.push_back(std::move(handle));
        }
        authorities.push_back(std::move(chain));
    }
    std::shared_ptr<Identity> identity;
    if (cert) {
        identity = std::make_shared<Identity>();
        identity->chain = ParseCertificate(*cert);
        const auto key = CopyInput(*privateKey);
        Random random;
        random.Seed();
        CheckBackend(mbedtls_pk_parse_key(&identity->key.key, key.bytes.get(), key.size, nullptr, 0,
            mbedtls_ctr_drbg_random, &random.drbg), "Invalid SSL private key");
        CheckBackend(mbedtls_pk_check_pair(&identity->chain->root.pk, &identity->key.key, mbedtls_ctr_drbg_random, &random.drbg),
            "SSL client certificate and private key do not match");
    }
    context.authorities.reserve(Add(context.authorities.size(), authorities.size()));
    context.certificateHandles.reserve(Add(context.certificateHandles.size(), handles.size()));
    for (auto& chain : authorities) context.authorities.push_back(std::move(chain));
    for (auto& handle : handles) context.certificateHandles.push_back(std::move(handle));
    if (identity) context.identity = std::move(identity);
    return 0;
}

int APS5_VABI sceSslUnloadCert(int sslCtxId) {
    auto& state = State();
    std::lock_guard lock(state.mutex);
    auto& context = GetContext(sslCtxId);
    context.authorities.clear();
    context.identity.reset();
    return 0;
}

int APS5_VABI sceSslGetCaList(int sslCtxId, SceSsl::CaList* output) {
    if (!output) return ErrorInvalidArg;
    auto& state = State();
    std::lock_guard lock(state.mutex);
    auto& context = GetContext(sslCtxId);
    PoolScope scope(context.pool);
    const auto count = CertificateCount(context);
    if (!count) {
        *output = {};
        return ErrorNotFound;
    }
    if (count > static_cast<std::size_t>(std::numeric_limits<int>::max())) throw std::length_error("Too many SSL authorities");
    Snapshot snapshot{Allocate(count * sizeof(void*)), context.authorities, count};
    auto** pointers = reinterpret_cast<void**>(snapshot.data.get());
    std::size_t index = 0;
    for (const auto& chain : snapshot.certificates) for (auto* handle : chain->handles) pointers[index++] = handle;
    context.caLists.emplace(pointers, std::move(snapshot));
    *output = {pointers, static_cast<int>(count)};
    return 0;
}

int APS5_VABI sceSslFreeCaList(int sslCtxId, SceSsl::CaList* list) {
    if (!list) return ErrorInvalidArg;
    auto& state = State();
    std::lock_guard lock(state.mutex);
    auto& context = GetContext(sslCtxId);
    if (!list->certs && list->num == 0) return 0;
    const auto found = context.caLists.find(list->certs);
    if (found == context.caLists.end() || list->num < 0 || static_cast<std::size_t>(list->num) != found->second.count)
        throw std::invalid_argument("Invalid SSL CA list allocation");
    context.caLists.erase(found);
    *list = {};
    return 0;
}

int APS5_VABI sceSslGetCaCerts(int sslCtxId, SceSsl::CaCerts* output) {
    if (!output) return ErrorInvalidArg;
    auto& state = State();
    std::lock_guard lock(state.mutex);
    const auto found = state.contexts.find(sslCtxId);
    if (found == state.contexts.end()) {
        *output = {};
        return ErrorNotFound;
    }
    auto& context = *found->second;
    PoolScope scope(context.pool);
    const auto count = CertificateCount(context);
    if (!count) {
        *output = {};
        return ErrorNotFound;
    }
    if (count > std::numeric_limits<std::size_t>::max() / sizeof(SceSsl::Data)) throw std::length_error("Too many SSL authorities");
    auto total = count * sizeof(SceSsl::Data);
    for (const auto& chain : context.authorities)
        for (auto* entry = &chain->root; entry; entry = entry->next) total = Add(total, entry->raw.len);
    Snapshot snapshot{Allocate(total), {}, count};
    auto* data = reinterpret_cast<SceSsl::Data*>(snapshot.data.get());
    auto* next = snapshot.data.get() + count * sizeof(SceSsl::Data);
    std::size_t index = 0;
    for (const auto& chain : context.authorities) {
        for (auto* entry = &chain->root; entry; entry = entry->next) {
            data[index++] = {next, entry->raw.len};
            std::memcpy(next, entry->raw.p, entry->raw.len);
            next += entry->raw.len;
        }
    }
    context.caCerts.emplace(data, std::move(snapshot));
    *output = {data, count, data};
    return 0;
}

int APS5_VABI sceSslFreeCaCerts(int sslCtxId, SceSsl::CaCerts* certificates) {
    if (!certificates) return ErrorInvalidArg;
    auto& state = State();
    std::lock_guard lock(state.mutex);
    const auto found = state.contexts.find(sslCtxId);
    if (found != state.contexts.end()) {
        auto& snapshots = found->second->caCerts;
        const auto allocation = snapshots.find(certificates->pool);
        if (allocation != snapshots.end()) {
            if (certificates->certs != certificates->pool || certificates->num != allocation->second.count)
                throw std::invalid_argument("Invalid SSL CA certificate allocation");
            snapshots.erase(allocation);
        }
    }
    *certificates = {};
    return 0;
}

int APS5_VABI sceSslCreateConnection(int sslCtxId, int sockId, const char* host) {
    if (!host) throw std::invalid_argument("sceSslCreateConnection: null host");
    std::lock_guard lock(State().mutex);
    if (!State().contexts.contains(sslCtxId)) return ErrorInvalidId;
    auto& state = State();
    if (state.nextHandle > static_cast<unsigned>(std::numeric_limits<int>::max())) throw std::overflow_error("SSL connection handles exhausted");
    const auto id = static_cast<int>(state.nextHandle);
    state.connections.emplace(id, SslConnection{sslCtxId, sockId, host, nullptr, nullptr});
    ++state.nextHandle;
    return id;
}

int APS5_VABI sceSslDeleteConnection(int sslConnectionId) {
    std::lock_guard lock(State().mutex);
    if (!State().connections.erase(sslConnectionId)) throw std::invalid_argument("sceSslDeleteConnection: unknown connection");
    return 0;
}

int APS5_VABI sceSslSetVerifyCallback(int sslConnectionId, SslVerifyCallback cbfunc, void* userArg) {
    std::lock_guard lock(State().mutex);
    SslConnection& connection = RequireConnection(__func__, sslConnectionId);
    connection.verifyCallback = cbfunc;
    connection.verifyArgument = userArg;
    return 0;
}

int APS5_VABI sceSslConnect(int sslConnectionId) {
    (void)sslConnectionId;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSslRead(int sslConnectionId, void* buf, size_t len, int flags) {
    (void)sslConnectionId;
    (void)buf;
    (void)len;
    (void)flags;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSslWrite(int sslConnectionId, void* buf, size_t len, int flags) {
    (void)sslConnectionId;
    (void)buf;
    (void)len;
    (void)flags;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSslClose(int sslConnectionId) {
    (void)sslConnectionId;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSslGetSerialNumber(int sslCtxId, void* sslCert, unsigned char* sboData, std::size_t* sboLen) {
    if (!sboLen) return ErrorInvalidArg;
    std::lock_guard lock(State().mutex);
    const auto* crt = RequireCertificate(__func__, sslCtxId, sslCert);
    const auto reqLen = crt->serial.len;
    if (!sboData || *sboLen == 0) {
        *sboLen = reqLen;
        return 0;
    }
    if (*sboLen < reqLen) {
        *sboLen = reqLen;
        return ErrorInvalidArg;
    }
    std::memcpy(sboData, crt->serial.p, reqLen);
    *sboLen = reqLen;
    return 0;
}

int APS5_VABI sceSslFreeSslCertName(int sslCtxId, void* certName) {
    if (!certName) return ErrorInvalidArg;
    auto& state = State();
    std::lock_guard lock(state.mutex);
    auto& context = GetContext(sslCtxId);
    const auto found = context.certNames.find(certName);
    if (found == context.certNames.end()) throw std::invalid_argument("sceSslFreeSslCertName: unknown cert name");
    context.certNames.erase(found);
    return 0;
}

int APS5_VABI sceSslEnableVerifyOption(int sslConnectionId, std::uint32_t option) {
    std::lock_guard lock(State().mutex);
    RequireConnection(__func__, sslConnectionId).verifyOptions |= option;
    return 0;
}

int APS5_VABI sceSslDisableVerifyOption(int sslConnectionId, std::uint32_t option) {
    std::lock_guard lock(State().mutex);
    RequireConnection(__func__, sslConnectionId).verifyOptions &= ~option;
    return 0;
}

const char* APS5_VABI sceSslGetAlpnSelected(int sslConnectionId) {
    std::lock_guard lock(State().mutex);
    RequireConnection(__func__, sslConnectionId);
    return nullptr;
}

int APS5_VABI sceSslSetAlpn(int sslConnectionId, const char* proto) {
    if (!proto) throw std::invalid_argument("sceSslSetAlpn: null protocol list");
    std::lock_guard lock(State().mutex);
    RequireConnection(__func__, sslConnectionId).alpn = proto;
    return 0;
}

int APS5_VABI sceSslSetMinSslVersion(int sslConnectionId, int version) {
    if (version < SslVersionNone || version > SslVersionTls12) {
        throw std::invalid_argument("sceSslSetMinSslVersion: unknown version " + std::to_string(version));
    }
    std::lock_guard lock(State().mutex);
    RequireConnection(__func__, sslConnectionId).minimumVersion = version;
    return 0;
}

void* APS5_VABI sceSslGetIssuerName(int sslCtxId, void* sslCert) {
    return ExtractCertName(__func__, sslCtxId, sslCert, false);
}

int APS5_VABI sceSslGetNameEntryCount(int sslCtxId, void* certName) {
    if (!certName) return ErrorInvalidArg;
    auto& state = State();
    std::lock_guard lock(state.mutex);
    auto& context = GetContext(sslCtxId);
    const auto found = context.certNames.find(certName);
    if (found == context.certNames.end()) throw std::invalid_argument("sceSslGetNameEntryCount: unknown cert name");
    return static_cast<int>(found->second->entries.size());
}

int APS5_VABI sceSslGetNameEntryInfo(int sslCtxId, void* certName, int entryNum, char* oidname, std::size_t maxOidnameLen,
    char* value, std::size_t maxValueLen, std::size_t* valueLen) {
    if (!certName) return ErrorInvalidArg;
    auto& state = State();
    std::lock_guard lock(state.mutex);
    auto& context = GetContext(sslCtxId);
    const auto found = context.certNames.find(certName);
    if (found == context.certNames.end()) throw std::invalid_argument("sceSslGetNameEntryInfo: unknown cert name");
    const auto& entries = found->second->entries;
    if (entryNum < 0 || static_cast<std::size_t>(entryNum) >= entries.size()) return ErrorInvalidArg;
    const auto& entry = entries[static_cast<std::size_t>(entryNum)];
    if (oidname && maxOidnameLen > 0) {
        const auto copyLen = std::min<std::size_t>(entry.oid.size(), maxOidnameLen - 1);
        std::memcpy(oidname, entry.oid.data(), copyLen);
        oidname[copyLen] = '\0';
    }
    if (value && maxValueLen > 0) {
        const auto copyLen = std::min<std::size_t>(entry.value.size(), maxValueLen - 1);
        std::memcpy(value, entry.value.data(), copyLen);
        value[copyLen] = '\0';
    }
    if (valueLen) *valueLen = entry.value.size();
    return 0;
}

int APS5_VABI sceSslGetNotBefore(int sslCtxId, void* sslCert, SceSsl::RtcTick* begin) {
    if (!begin) return ErrorInvalidArg;
    std::lock_guard lock(State().mutex);
    const auto* crt = RequireCertificate(__func__, sslCtxId, sslCert);
    begin->tick = TimeToTick(crt->valid_from);
    return 0;
}

int APS5_VABI sceSslGetNotAfter(int sslCtxId, void* sslCert, SceSsl::RtcTick* limit) {
    if (!limit) return ErrorInvalidArg;
    std::lock_guard lock(State().mutex);
    const auto* crt = RequireCertificate(__func__, sslCtxId, sslCert);
    limit->tick = TimeToTick(crt->valid_to);
    return 0;
}

int APS5_VABI sceSslGetFingerprint(int sslCtxId, void* sslCert, int hashType, unsigned char* fingerprint) {
    if (!fingerprint) return ErrorInvalidArg;
    mbedtls_md_type_t mdType;
    switch (hashType) {
    case 0: mdType = MBEDTLS_MD_MD5; break;
    case 1: mdType = MBEDTLS_MD_SHA1; break;
    case 2: mdType = MBEDTLS_MD_SHA224; break;
    case 3: mdType = MBEDTLS_MD_SHA256; break;
    case 4: mdType = MBEDTLS_MD_SHA384; break;
    case 5: mdType = MBEDTLS_MD_SHA512; break;
    default: return ErrorInvalidArg;
    }
    const auto* info = mbedtls_md_info_from_type(mdType);
    if (!info) return ErrorInvalidArg;
    std::lock_guard lock(State().mutex);
    const auto* crt = RequireCertificate(__func__, sslCtxId, sslCert);
    if (mbedtls_md(info, crt->raw.p, crt->raw.len, fingerprint) != 0) return ErrorInvalidArg;
    return 0;
}

int APS5_VABI sceSslGetPem(int sslCtxId, void* sslCert, char* buf, std::size_t* bufLen) {
    if (!bufLen) return ErrorInvalidArg;
    std::lock_guard lock(State().mutex);
    auto& context = GetContext(sslCtxId);
    const auto* crt = RequireCertificate(__func__, sslCtxId, sslCert);
    PoolScope scope(context.pool);
    std::size_t required = 0;
    const auto encode = [&](unsigned char* output, std::size_t capacity, std::size_t* written) {
        return mbedtls_pem_write_buffer("-----BEGIN CERTIFICATE-----\n", "-----END CERTIFICATE-----\n", crt->raw.p, crt->raw.len, output, capacity, written);
    };
    const int sizing = encode(nullptr, 0, &required);
    if (sizing != MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL) CheckBackend(sizing, "SSL PEM sizing failed");
    if (!buf) {
        *bufLen = required;
        return 0;
    }
    if (*bufLen < required) throw std::length_error("SSL PEM buffer is too small");
    std::size_t written = 0;
    CheckBackend(encode(reinterpret_cast<unsigned char*>(buf), required, &written), "SSL PEM encoding failed");
    *bufLen = written;
    return 0;
}

void* APS5_VABI sceSslGetSubjectName(int sslCtxId, void* sslCert) {
    return ExtractCertName(__func__, sslCtxId, sslCert, true);
}

}
