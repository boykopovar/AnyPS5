#include "prx/libSceLibreSsl/libSceLibreSsl.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/FileStream.hpp"
#include "Testing.hpp"
#include <array>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <string_view>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

void* APS5_VABI Allocate(std::size_t size) { return std::malloc(size); }
void APS5_VABI Free(void* pointer) { std::free(pointer); }
void* APS5_VABI Calloc(std::size_t count, std::size_t size) { return std::calloc(count, size); }
void* APS5_VABI Reallocate(void* pointer, std::size_t size) { return std::realloc(pointer, size); }
void* APS5_VABI Align(std::size_t, std::size_t) { throw std::logic_error("unexpected aligned allocation"); }
void* APS5_VABI Realign(void*, std::size_t, std::size_t) { throw std::logic_error("unexpected realignment"); }
int APS5_VABI PosixAlign(void**, std::size_t, std::size_t) { throw std::logic_error("unexpected aligned allocation"); }

void Check(std::string_view input, int capacity, int length, long consumed, bool readToEnd) {
    auto* native = std::tmpfile();
    auto* reference = std::tmpfile();
    Require(native && reference, "create file BIO fixtures");
    for (auto* file : {native, reference}) {
        Require(std::fwrite(input.data(), 1, input.size(), file) == input.size(), "write file BIO fixture");
        std::rewind(file);
    }
    FileStream stream(native);
    auto* bio = BIO_new_fp_nid_postfix(&stream, 0);
    Require(bio && bio->method == BIO_s_file_nid_postfix(), "file BIO constructor installs file gets method");
    std::array<char, 32> actual;
    actual.fill('!');
    ERR_clear_error_nid_postfix();
    const int result = bio->method->gets(bio, actual.data(), capacity);
    Require(std::ftell(native) == consumed, "file gets stream position matches fgets consumption");
    if (result != length) std::fprintf(stderr, "file gets length: actual=%d expected=%d consumed=%ld\n", result, length, consumed);
    Require(result == length, "file gets returns strlen rather than consumed bytes");
    Require(actual[0] == (length ? input[0] : 0), "file gets clears the first byte even on empty results");
    if (capacity >= 2) {
        std::array<char, 32> expected;
        expected.fill('!');
        expected[0] = 0;
        const auto line = std::fgets(expected.data(), capacity, reference);
        Require(result == (line ? static_cast<int>(std::strlen(expected.data())) : 0), "file gets matches fgets string length");
        Require(actual == expected, "file gets preserves bytes beyond embedded NUL and bounds");
        Require(std::ftell(native) == std::ftell(reference), "file gets matches reference stream position");
    } else {
        Require(actual[1] == '!', "small capacity writes only the initial NUL");
    }
    Require(bio->bytesRead == 0, "file gets method does not update BIO read accounting");
    if (readToEnd) {
        const auto remaining = input.size() - static_cast<std::size_t>(consumed);
        std::array<char, 32> rest{};
        Require(bio->method->gets(bio, rest.data(), static_cast<int>(rest.size())) == static_cast<int>(remaining), "next gets reads the unconsumed suffix");
        Require(std::memcmp(rest.data(), input.data() + consumed, remaining) == 0, "next gets suffix contents");
        Require(std::ftell(native) == static_cast<long>(input.size()), "next gets final position");
        rest.fill('!');
        Require(bio->method->gets(bio, rest.data(), static_cast<int>(rest.size())) == 0 && rest[0] == 0, "EOF gets returns zero and clears the first byte");
        Require(std::ftell(native) == static_cast<long>(input.size()), "EOF does not advance position");
    }
    Require(ERR_get_error_nid_postfix() == 0, "file gets queues no BIO errors");
    Require(BIO_free_nid_postfix(bio) == 1, "free borrowed file BIO");
    Require(std::fclose(native) == 0 && std::fclose(reference) == 0, "close file BIO fixtures");
}

void CheckReadError() {
    int descriptors[2]{-1, -1};
#ifdef _WIN32
    Require(_pipe(descriptors, 4096, _O_BINARY) == 0, "create read-error pipe");
    auto* native = _fdopen(descriptors[1], "wb");
#else
    Require(::pipe(descriptors) == 0, "create read-error pipe");
    auto* native = ::fdopen(descriptors[1], "wb");
#endif
    Require(native != nullptr, "create read-error fixture");
    FileStream stream(native);
    auto* bio = BIO_new_fp_nid_postfix(&stream, 0);
    Require(bio != nullptr, "create read-error BIO");
    char output[16] = "sentinel";
    ERR_clear_error_nid_postfix();
    Require(bio->method->gets(bio, output, sizeof(output)) == 0 && output[0] == 0, "failed fgets returns zero and clears buffer");
    Require(ERR_get_error_nid_postfix() == 0, "failed fgets does not publish FileRead errors");
    Require(ferror_nid_postfix(&stream) != 0 && feof_nid_postfix(&stream) == 0, "failed fgets sets stream error without EOF");
    Require(BIO_free_nid_postfix(bio) == 1, "free read-error BIO");
    Require(stream.Close() == 0, "close read-error writer");
#ifdef _WIN32
    Require(_close(descriptors[0]) == 0, "close read-error reader");
#else
    Require(::close(descriptors[0]) == 0, "close read-error reader");
#endif
}

void RequireInvalidArgument(LibreSsl::Bio* bio, char* data, int size, const char* message) {
    try {
        bio->method->gets(bio, data, size);
    } catch (const std::invalid_argument&) {
        return;
    }
    Require(false, message);
}

void CheckNullArguments() {
    auto* unbound = BIO_new_nid_postfix(BIO_s_file_nid_postfix());
    Require(unbound && !unbound->data, "create file BIO without a stream");
    char output[4] = "abc";
    RequireInvalidArgument(unbound, output, sizeof(output), "file gets without a stream throws");
    Require(BIO_free_nid_postfix(unbound) == 1, "free unbound file BIO");
    auto* native = std::tmpfile();
    Require(native && std::fwrite("AB\n", 1, 3, native) == 3, "write null-buffer fixture");
    std::rewind(native);
    FileStream stream(native);
    auto* bio = BIO_new_fp_nid_postfix(&stream, 0);
    Require(bio != nullptr, "create null-buffer BIO");
    RequireInvalidArgument(bio, nullptr, 4, "file gets with a null buffer throws");
    Require(std::ftell(native) == 0, "null-buffer gets does not consume input");
    Require(BIO_free_nid_postfix(bio) == 1, "free null-buffer BIO");
    Require(std::fclose(native) == 0, "close null-buffer fixture");
}

class GuardedPage {
public:
    GuardedPage() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        bytes = info.dwPageSize;
        data = static_cast<char*>(VirtualAlloc(nullptr, bytes * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        Require(data != nullptr, "guarded page allocation");
        DWORD previous{};
        Require(VirtualProtect(data + bytes, bytes, PAGE_NOACCESS, &previous) != 0, "guard page protection");
#else
        bytes = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
        void* mapped = mmap(nullptr, bytes * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(mapped != MAP_FAILED, "guarded page mapping");
        data = static_cast<char*>(mapped);
        Require(mprotect(data + bytes, bytes, PROT_NONE) == 0, "guard page protection");
#endif
    }

    ~GuardedPage() {
#ifdef _WIN32
        VirtualFree(data, 0, MEM_RELEASE);
#else
        munmap(data, bytes * 2);
#endif
    }

    GuardedPage(const GuardedPage&) = delete;
    GuardedPage& operator=(const GuardedPage&) = delete;

    char* Tail(const char* value, std::size_t size) {
        std::memcpy(data + bytes - size, value, size);
        return data + bytes - size;
    }

    void ReadOnly() {
#ifdef _WIN32
        DWORD previous{};
        Require(VirtualProtect(data, bytes, PAGE_READONLY, &previous) != 0, "read-only page protection");
#else
        Require(mprotect(data, bytes, PROT_READ) == 0, "read-only page protection");
#endif
    }

private:
    char* data = nullptr;
    std::size_t bytes = 0;
};

template<class TFunction>
void RequireRejected(TFunction function, const char* message) {
    errno = 71;
    bool rejected = false;
    try {
        function();
    } catch (const std::runtime_error&) {
        rejected = errno == 71;
    }
    Require(rejected, message);
}

void CheckGuestPointers() {
    auto* unmapped = reinterpret_cast<char*>(0x10);
    GuardedPage page;
    auto* native = std::tmpfile();
    Require(native && std::fwrite("AB\nCD\n", 1, 6, native) == 6, "write guest pointer fixture");
    std::rewind(native);
    FileStream stream(native);
    auto* bio = BIO_new_fp_nid_postfix(&stream, 0);
    Require(bio != nullptr, "create guest pointer file BIO");
    RequireRejected([&] { bio->method->gets(bio, unmapped, 4); }, "unmapped gets destination");
    RequireRejected([&] { bio->method->gets(bio, page.Tail("...", 3), 4); }, "gets destination running into an unmapped page");
    Require(std::ftell(native) == 0, "rejected gets destinations consume nothing");
    char* line = page.Tail("!!!!", 4);
    Require(bio->method->gets(bio, line, 4) == 3 && std::memcmp(line, "AB\n", 4) == 0, "gets destination ending at a page boundary");
    RequireRejected([&] { BIO_ctrl_nid_postfix(bio, 107, 0, unmapped); }, "unmapped stream output");
    FileStream* queried = nullptr;
    Require(BIO_ctrl_nid_postfix(bio, 107, 0, &queried) == 1 && queried == &stream, "stream output after a rejected destination");

    auto* written = std::tmpfile();
    Require(written != nullptr, "create puts fixture");
    FileStream writtenStream(written);
    auto* writer = BIO_new_fp_nid_postfix(&writtenStream, 0);
    Require(writer != nullptr, "create puts file BIO");
    RequireRejected([&] { BIO_puts_nid_postfix(writer, unmapped); }, "unmapped file puts string");
    RequireRejected([&] { BIO_puts_nid_postfix(writer, page.Tail("zz", 2)); }, "unterminated file puts string running into an unmapped page");
    Require(BIO_puts_nid_postfix(writer, page.Tail("xy", 3)) == 2, "file puts string ending at a page boundary");
    Require(BIO_ctrl_nid_postfix(writer, 11, 0, nullptr) == 1, "flush puts file BIO");
    std::rewind(written);
    char contents[8]{};
    Require(std::fread(contents, 1, sizeof(contents), written) == 2 && std::string(contents) == "xy", "file puts writes only the accepted string");
    Require(BIO_free_nid_postfix(writer) == 1, "free puts file BIO");
    Require(std::fclose(written) == 0, "close puts fixture");

    auto* socket = BIO_new_socket_nid_postfix(42, 0);
    Require(socket != nullptr, "create guest pointer socket BIO");
    RequireRejected([&] { BIO_puts_nid_postfix(socket, unmapped); }, "unmapped socket puts string");
    RequireRejected([&] { BIO_puts_nid_postfix(socket, page.Tail("zz", 2)); }, "unterminated socket puts string running into an unmapped page");
    Require(BIO_free_nid_postfix(socket) == 1, "free guest pointer socket BIO");

    line = page.Tail("!!!!", 4);
    page.ReadOnly();
    RequireRejected([&] { bio->method->gets(bio, line, 4); }, "read-only gets destination");
    Require(std::ftell(native) == 3 && std::memcmp(line, "!!!!", 4) == 0, "rejected read-only destination consumes nothing");
    Require(BIO_free_nid_postfix(bio) == 1, "free guest pointer file BIO");
    Require(std::fclose(native) == 0, "close guest pointer fixture");
}

}  // namespace

int main(int argc, char** argv) {
    const std::array<void*, 10> api{
        reinterpret_cast<void*>(Allocate), reinterpret_cast<void*>(Free), reinterpret_cast<void*>(Calloc),
        reinterpret_cast<void*>(Reallocate), reinterpret_cast<void*>(Align), reinterpret_cast<void*>(Realign),
        reinterpret_cast<void*>(PosixAlign)};
    ApplicationHeapRegister_nid_no_patch(api.data());
    Require(argc == 2, "select file gets case");
    const std::string_view name(argv[1]);
    if (name == "leading_nul") Check(std::string_view("\0AB\nZ", 5), 16, 0, 4, true);
    else if (name == "interior_nul") Check(std::string_view("A\0B\nZ", 5), 16, 1, 4, true);
    else if (name == "ordinary") Check("ABC\nZ", 16, 4, 4, true);
    else if (name == "truncated") Check("ABCDEF\n", 4, 3, 3, true);
    else if (name == "capacity_one") Check("AB\n", 1, 0, 0, true);
    else if (name == "capacity_two") Check("AB\n", 2, 1, 1, true);
    else if (name == "eof") Check("ABC", 16, 3, 3, true);
    else if (name == "empty") Check("", 16, 0, 0, true);
    else if (name == "nonpositive") {
        Check("ABC", 0, 0, 0, true);
        Check("ABC", -1, 0, 0, true);
    } else if (name == "read_error") CheckReadError();
    else if (name == "null_arguments") CheckNullArguments();
    else if (name == "guest_pointers") CheckGuestPointers();
    else Require(false, "unknown file gets case");
}
