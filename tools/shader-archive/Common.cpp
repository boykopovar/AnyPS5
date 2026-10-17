#include "Common.hpp"
#include <limits>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <psapi.h>
#else
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <unistd.h>
#endif

namespace ShaderArchive {
Json::Value ProcessUsage() {
    Json::Value usage;
#ifdef _WIN32
    FILETIME created{}, exited{}, kernel{}, user{};
    if (GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user)) {
        auto seconds = [](FILETIME t) { return (double(t.dwHighDateTime) * 4294967296.0 + t.dwLowDateTime) * 1e-7; };
        usage["user_seconds"] = seconds(user);
        usage["kernel_seconds"] = seconds(kernel);
    }
    PROCESS_MEMORY_COUNTERS memory{};
    memory.cb = sizeof(memory);
    if (GetProcessMemoryInfo(GetCurrentProcess(), &memory, sizeof(memory)))
        usage["peak_resident_bytes"] = Json::UInt64(memory.PeakWorkingSetSize);
#else
    rusage r{};
    if (getrusage(RUSAGE_SELF, &r) == 0) {
        usage["user_seconds"] = double(r.ru_utime.tv_sec) + r.ru_utime.tv_usec * 1e-6;
        usage["kernel_seconds"] = double(r.ru_stime.tv_sec) + r.ru_stime.tv_usec * 1e-6;
        usage["peak_resident_bytes"] = Json::UInt64(r.ru_maxrss) * 1024;
    }
#endif
    return usage;
}
View Slice(View d, std::size_t at, std::size_t n) {
    Check(at <= d.size() && n <= d.size() - at, "truncated binary range");
    return d.subspan(at, n);
}
std::uint64_t ReadInt(View d, std::size_t at, unsigned n, bool big) {
    Check(n <= 8, "integer width");
    auto s = Slice(d, at, n);
    std::uint64_t v = 0;
    for (unsigned i = 0; i < n; ++i)
        v |= std::uint64_t(s[i]) << (8 * (big ? n - i - 1 : i));
    return v;
}
std::string Hex(View d) {
    static constexpr char digits[] = "0123456789abcdef";
    std::string out(d.size() * 2, '0');
    for (std::size_t i = 0; i < d.size(); ++i) {
        out[i * 2] = digits[d[i] >> 4];
        out[i * 2 + 1] = digits[d[i] & 15];
    }
    return out;
}
std::string Sha256(View d) {
    using U32 = std::uint32_t;
    static constexpr std::array<U32, 64> k{
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
        0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
        0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
        0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
        0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
    std::array<U32, 8> h{0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                         0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    const auto rotr = [](U32 x, unsigned n) { return (x >> n) | (x << (32 - n)); };
    const std::uint64_t bitLen = static_cast<std::uint64_t>(d.size()) * 8;
    const auto block = [&](const std::uint8_t *p) {
        std::array<U32, 64> w{};
        for (unsigned i = 0; i < 16; ++i)
            w[i] = (U32(p[4 * i]) << 24) | (U32(p[4 * i + 1]) << 16) | (U32(p[4 * i + 2]) << 8) | U32(p[4 * i + 3]);
        for (unsigned i = 16; i < 64; ++i) {
            const U32 s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
            const U32 s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }
        U32 a = h[0], b = h[1], c = h[2], e = h[4], f = h[5], g = h[6], hh = h[7];
        U32 dd = h[3];
        for (unsigned i = 0; i < 64; ++i) {
            const U32 s1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
            const U32 ch = (e & f) ^ (~e & g);
            const U32 t1 = hh + s1 + ch + k[i] + w[i];
            const U32 s0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
            const U32 mj = (a & b) ^ (a & c) ^ (b & c);
            const U32 t2 = s0 + mj;
            hh = g;
            g = f;
            f = e;
            e = dd + t1;
            dd = c;
            c = b;
            b = a;
            a = t1 + t2;
        }
        h = {h[0] + a, h[1] + b, h[2] + c, h[3] + dd, h[4] + e, h[5] + f, h[6] + g, h[7] + hh};
    };
    std::size_t at = 0;
    while (at + 64 <= d.size()) {
        block(d.data() + at);
        at += 64;
    }
    std::array<std::uint8_t, 128> tail{};
    const std::size_t rem = d.size() - at;
    std::memcpy(tail.data(), d.data() + at, rem);
    tail[rem] = 0x80;
    const std::size_t tailBlocks = rem < 56 ? 64 : 128;
    for (unsigned i = 0; i < 8; ++i)
        tail[tailBlocks - 1 - i] = static_cast<std::uint8_t>(bitLen >> (8 * i));
    block(tail.data());
    if (tailBlocks == 128)
        block(tail.data() + 64);
    std::array<std::uint8_t, 32> out{};
    for (unsigned i = 0; i < 8; ++i)
        for (unsigned j = 0; j < 4; ++j)
            out[4 * i + j] = static_cast<std::uint8_t>(h[i] >> (24 - 8 * j));
    return Hex(out);
}
Bytes ReadFile(const fs::path &p) {
    std::ifstream f(p, std::ios::binary | std::ios::ate);
    Check(bool(f), "open " + p.string());
    auto n = f.tellg();
    Check(n >= 0, "size " + p.string());
    Bytes out(static_cast<std::size_t>(n));
    f.seekg(0);
    f.read(reinterpret_cast<char *>(out.data()), n);
    Check(bool(f), "read " + p.string());
    return out;
}
void WriteFile(const fs::path &p, View d) {
    std::ofstream f(p, std::ios::binary | std::ios::trunc);
    Check(bool(f), "create " + p.string());
    f.write(reinterpret_cast<const char *>(d.data()), static_cast<std::streamsize>(d.size()));
    f.close();
    Check(bool(f), "write " + p.string());
}
void WriteText(const fs::path &p, const std::string &s) {
    WriteFile(p, {reinterpret_cast<const std::uint8_t *>(s.data()), s.size()});
}
void WriteAtomic(const fs::path &p, View d) {
    static std::atomic_uint64_t next{0};
#ifdef _WIN32
    const auto pid = GetCurrentProcessId();
#else
    const auto pid = getpid();
#endif
    auto temp = p;
    temp += ".tmp." + std::to_string(pid) + "." + std::to_string(next.fetch_add(1));
    WriteFile(temp, d);
    PublishAtomic(temp, p);
}
void PublishAtomic(const fs::path &temp, const fs::path &p) {
#ifdef _WIN32
    if (!MoveFileExW(temp.c_str(), p.c_str(), MOVEFILE_REPLACE_EXISTING)) {
        fs::remove(temp);
        throw std::runtime_error("publish cache entry " + p.string());
    }
#else
    fs::rename(temp, p);
#endif
}
std::string JsonText(const Json::Value &v) {
    Json::StreamWriterBuilder b;
    b["indentation"] = "";
    return Json::writeString(b, v);
}
Json::Value ParseJson(std::string_view s) {
    Json::CharReaderBuilder b;
    b["collectComments"] = false;
    b["failIfExtra"] = true;
    b["rejectDupKeys"] = true;
    std::unique_ptr<Json::CharReader> r(b.newCharReader());
    Json::Value v;
    std::string error;
    const bool parsed = r->parse(s.data(), s.data() + s.size(), &v, &error);
    Check(parsed, "JSON: " + error);
    return v;
}
Json::Value ReadJson(const fs::path &p) {
    auto bytes = ReadFile(p);
    return ParseJson({reinterpret_cast<const char *>(bytes.data()), bytes.size()});
}
void WriteJson(const fs::path &p, const Json::Value &v) {
    WriteText(p, JsonText(v) + "\n");
}
void Parallel(std::size_t n, unsigned workers, const std::function<void(std::size_t, unsigned)> &fn) {
    Check(workers > 0 && workers <= 256, "workers must be 1..256");
    std::atomic_size_t next{0};
    std::atomic_bool stop{false};
    std::exception_ptr error;
    std::mutex mutex;
    std::vector<std::jthread> threads;
    for (unsigned worker = 0; worker < std::min<std::size_t>(n, workers); ++worker)
        threads.emplace_back([&, worker] {
            try {
                while (!stop.load()) {
                    const auto i = next.fetch_add(1);
                    if (i >= n)
                        break;
                    fn(i, worker);
                }
            } catch (...) {
                std::lock_guard lock(mutex);
                if (!error)
                    error = std::current_exception();
                stop = true;
            }
        });
    threads.clear();
    if (error)
        std::rethrow_exception(error);
}
MappedFile::MappedFile(const fs::path &p) {
    const auto n = fs::file_size(p);
    Check(n <= std::numeric_limits<std::size_t>::max(), "mapping too large");
    size = static_cast<std::size_t>(n);
    if (!size)
        return;
#ifdef _WIN32
    file =
        CreateFileW(p.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    Check(file != INVALID_HANDLE_VALUE, "open mapping " + p.string());
    mapping = CreateFileMappingW(file, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (mapping)
        data = static_cast<const std::uint8_t *>(MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0));
    if (!data) {
        if (mapping)
            CloseHandle(mapping);
        CloseHandle(file);
        throw std::runtime_error("map " + p.string());
    }
#else
    fd = open(p.c_str(), O_RDONLY);
    Check(fd >= 0, "open mapping " + p.string());
    auto *ptr = mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (ptr == MAP_FAILED) {
        close(fd);
        throw std::runtime_error("map " + p.string());
    }
    data = static_cast<const std::uint8_t *>(ptr);
#endif
}
MappedFile::~MappedFile() {
#ifdef _WIN32
    if (data)
        UnmapViewOfFile(data);
    if (mapping)
        CloseHandle(mapping);
    if (file && file != INVALID_HANDLE_VALUE)
        CloseHandle(file);
#else
    if (data)
        munmap(const_cast<std::uint8_t *>(data), size);
    if (fd >= 0)
        close(fd);
#endif
}
std::string Reader::String() {
    const auto n = static_cast<std::int32_t>(Word());
    if (!n)
        return {};
    if (n > 0) {
        auto s = Take(n);
        Check(s.back() == 0, "unterminated FString");
        return {reinterpret_cast<const char *>(s.data()), s.size() - 1};
    }
    auto s = Take(std::size_t(-std::int64_t(n)) * 2);
    Check(U16(s, s.size() - 2) == 0, "unterminated wide FString");
    std::string out;
    for (std::size_t i = 0; i + 2 < s.size(); i += 2) {
        auto c = U16(s, i);
        Check(c < 128, "non-ASCII wide directory name");
        out.push_back(static_cast<char>(c));
    }
    return out;
}
} // namespace ShaderArchive
