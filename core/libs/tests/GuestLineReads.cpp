#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <string>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

void Require(bool condition, const char* message = "line read check failed") {
    if (!condition) throw std::runtime_error(message);
}

void* APS5_VABI Allocate(std::size_t size) { return std::malloc(size); }
void APS5_VABI Free(void* pointer) { std::free(pointer); }
void* APS5_VABI Calloc(std::size_t count, std::size_t size) { return std::calloc(count, size); }
void* APS5_VABI Reallocate(void* pointer, std::size_t size) { return std::realloc(pointer, size); }
void* APS5_VABI AlignedAllocate(std::size_t, std::size_t size) { return Allocate(size); }
void* APS5_VABI Realign(void* pointer, std::size_t size, std::size_t) { return Reallocate(pointer, size); }
int APS5_VABI PosixAlign(void** pointer, std::size_t, std::size_t size) {
    if (pointer == nullptr) return 22;
    *pointer = Allocate(size);
    return *pointer == nullptr ? 12 : 0;
}

void RegisterAllocator() {
    std::array<void*, 10> api{};
    api[0] = reinterpret_cast<void*>(&Allocate);
    api[1] = reinterpret_cast<void*>(&Free);
    api[2] = reinterpret_cast<void*>(&Calloc);
    api[3] = reinterpret_cast<void*>(&Reallocate);
    api[4] = reinterpret_cast<void*>(&AlignedAllocate);
    api[5] = reinterpret_cast<void*>(&Realign);
    api[6] = reinterpret_cast<void*>(&PosixAlign);
    ApplicationHeapRegister_nid_no_patch(api.data());
}

}

#ifdef _WIN32
namespace {
std::size_t PageSize() {
    SYSTEM_INFO info{};
    GetSystemInfo(&info);
    return info.dwPageSize;
}
void* PageAlloc(std::size_t size) { return VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE); }
bool PageProtect(void* address, std::size_t size, bool writable) {
    DWORD previous = 0;
    return VirtualProtect(address, size, writable ? PAGE_READWRITE : PAGE_READONLY, &previous) != 0;
}
void PageFree(void* address) { VirtualFree(address, 0, MEM_RELEASE); }
}
#else
namespace {
std::size_t PageSize() { return static_cast<std::size_t>(sysconf(_SC_PAGESIZE)); }
void* PageAlloc(std::size_t size) {
    void* mapped = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    return mapped == MAP_FAILED ? nullptr : mapped;
}
bool PageProtect(void* address, std::size_t size, bool writable) {
    return mprotect(address, size, writable ? (PROT_READ | PROT_WRITE) : PROT_READ) == 0;
}
void PageFree(void* address) { munmap(address, PageSize()); }
}
#endif

int main(int argc, char** argv) {
    Require(argc == 2);
    RegisterAllocator();

    const std::string base = argv[1];
    const std::string path = base + ".lines";
    auto* stream = fopen_nid_postfix(path.c_str(), "w+b");
    Require(stream != nullptr);
    constexpr char text[] = "alpha\nbeta;gamma\ntail";
    Require(fwrite_nid_postfix(text, 1, sizeof(text) - 1, stream) == sizeof(text) - 1);
    Require(fflush_nid_postfix(stream) == 0);
    Require(fseek_nid_postfix(stream, 0, SEEK_SET) == 0);

    char* line = nullptr;
    std::size_t capacity = 0;
    Require(getline_nid_postfix(&line, &capacity, stream) == 6);
    Require(std::strcmp(line, "alpha\n") == 0 && capacity >= 7);
    Require(getdelim_nid_postfix(&line, &capacity, ';', stream) == 5);
    Require(std::strcmp(line, "beta;") == 0);
    Require(getdelim_nid_postfix(&line, &capacity, '\n', stream) == 6);
    Require(std::strcmp(line, "gamma\n") == 0);
    Require(getline_nid_postfix(&line, &capacity, stream) == 4);
    Require(std::strcmp(line, "tail") == 0);
    char* const remaining = line;
    Require(getline_nid_postfix(&line, &capacity, stream) == -1);
    Require(line == remaining && feof_nid_postfix(stream) != 0 && ferror_nid_postfix(stream) == 0);
    std::strcpy(line, "xyz");
    Require(getline_nid_postfix(&line, &capacity, stream) == -1 && line == remaining && line[0] == '\0');
    errno = 0;
    Require(getline_nid_postfix(nullptr, &capacity, stream) == -1 && errno == EINVAL);
    Require(getline_nid_postfix(&line, nullptr, stream) == -1 && errno == EINVAL);
    Require(getline_nid_postfix(&line, &capacity, nullptr) == -1 && errno == EINVAL);
    Require(fclose_nid_postfix(stream) == 0);

    const std::string nulPath = base + ".nul";
    stream = fopen_nid_postfix(nulPath.c_str(), "w+b");
    Require(stream != nullptr);
    constexpr char embedded[] = {'x', '\0', 'y', '\n'};
    Require(fwrite_nid_postfix(embedded, 1, sizeof(embedded), stream) == sizeof(embedded));
    Require(fseek_nid_postfix(stream, 0, SEEK_SET) == 0);
    Require(getline_nid_postfix(&line, &capacity, stream) == 4);
    Require(std::memcmp(line, embedded, sizeof(embedded)) == 0);
    Require(fclose_nid_postfix(stream) == 0);

    const std::string emptyPath = base + ".empty";
    stream = fopen_nid_postfix(emptyPath.c_str(), "w+b");
    Require(stream != nullptr);
    char* empty = nullptr;
    std::size_t emptyCapacity = 0;
    Require(getline_nid_postfix(&empty, &emptyCapacity, stream) == -1);
    Require(empty != nullptr && empty[0] == '\0' && emptyCapacity == 1);
    Require(fclose_nid_postfix(stream) == 0);
    free_nid_postfix(empty);
    Require(std::filesystem::remove(emptyPath));

    const std::string zeroPath = base + ".zero";
    stream = fopen_nid_postfix(zeroPath.c_str(), "w+b");
    Require(stream != nullptr);
    char keptBuffer[4] = "xyz";
    char* zeroLine = keptBuffer;
    std::size_t zeroCapacity = 0;
    Require(getline_nid_postfix(&zeroLine, &zeroCapacity, stream) == -1);
    Require(zeroLine != keptBuffer && zeroLine[0] == '\0' && zeroCapacity == 1);
    free_nid_postfix(zeroLine);
    Require(fwrite_nid_postfix("ab\n", 1, 3, stream) == 3);
    Require(fseek_nid_postfix(stream, 0, SEEK_SET) == 0);
    char* zeroRead = keptBuffer;
    zeroCapacity = 0;
    Require(getline_nid_postfix(&zeroRead, &zeroCapacity, stream) == 3);
    Require(zeroRead != keptBuffer && std::memcmp(zeroRead, "ab\n", 4) == 0 && zeroCapacity == 4);
    Require(fclose_nid_postfix(stream) == 0);
    free_nid_postfix(zeroRead);
    Require(std::filesystem::remove(zeroPath));

    const std::string highPath = base + ".high";
    stream = fopen_nid_postfix(highPath.c_str(), "w+b");
    Require(stream != nullptr);
    constexpr char high[] = {'a', static_cast<char>(0xE9), 'b', '\n'};
    Require(fwrite_nid_postfix(high, 1, sizeof(high), stream) == sizeof(high));
    Require(fseek_nid_postfix(stream, 0, SEEK_SET) == 0);
    char* highLine = nullptr;
    std::size_t highCapacity = 0;
    Require(getdelim_nid_postfix(&highLine, &highCapacity, static_cast<char>(0xE9), stream) == 2);
    Require(std::memcmp(highLine, high, 2) == 0);
    Require(fclose_nid_postfix(stream) == 0);
    free_nid_postfix(highLine);
    Require(std::filesystem::remove(highPath));

    const auto page = PageSize();
    auto* lineStorage = static_cast<std::byte*>(PageAlloc(page));
    auto* capacityStorage = static_cast<std::byte*>(PageAlloc(page));
    Require(lineStorage != nullptr && capacityStorage != nullptr);
    auto** linePointer = reinterpret_cast<char**>(lineStorage);
    auto* capacityPointer = reinterpret_cast<std::size_t*>(capacityStorage);
    char* kept = static_cast<char*>(realloc_nid_postfix(nullptr, 4));
    Require(kept != nullptr);
    std::memcpy(kept, "old", 4);
    *linePointer = kept;
    *capacityPointer = 4;
    const std::string growPath = base + ".grow";
    stream = fopen_nid_postfix(growPath.c_str(), "w+b");
    Require(stream != nullptr && fwrite_nid_postfix("longer line\n", 1, 12, stream) == 12);
    Require(fseek_nid_postfix(stream, 0, SEEK_SET) == 0);

    Require(PageProtect(lineStorage, page, false));
    errno = 0;
    Require(getline_nid_postfix(linePointer, capacityPointer, stream) == -1 && errno == EFAULT);
    Require(*linePointer == kept && *capacityPointer == 4 && std::memcmp(kept, "old", 4) == 0);

    Require(PageProtect(lineStorage, page, true));
    Require(PageProtect(capacityStorage, page, false));
    Require(fseek_nid_postfix(stream, 0, SEEK_SET) == 0);
    errno = 0;
    Require(getline_nid_postfix(linePointer, capacityPointer, stream) == -1 && errno == EFAULT);
    Require(*linePointer == kept && *capacityPointer == 4 && std::memcmp(kept, "old", 4) == 0);

    Require(PageProtect(capacityStorage, page, true));
    Require(fclose_nid_postfix(stream) == 0);
    Require(std::filesystem::remove(growPath));
    PageFree(lineStorage);
    PageFree(capacityStorage);
    free_nid_postfix(kept);


    {
        const auto cellPage = PageSize();
        auto* pointerStorage = static_cast<std::byte*>(PageAlloc(cellPage));
        auto* capacityStorage = static_cast<std::byte*>(PageAlloc(cellPage));
        Require(pointerStorage != nullptr && capacityStorage != nullptr);
        auto** emptyLinePointer = reinterpret_cast<char**>(pointerStorage);
        auto* emptyCapacity = reinterpret_cast<std::size_t*>(capacityStorage);
        *emptyLinePointer = nullptr;
        *emptyCapacity = 0;
        const std::string emptyPath = base + ".eofcell";
        auto* eofStream = fopen_nid_postfix(emptyPath.c_str(), "w+b");
        Require(eofStream != nullptr);
        Require(PageProtect(pointerStorage, cellPage, false));
        errno = 0;
        Require(getdelim_nid_postfix(emptyLinePointer, emptyCapacity, '\n', eofStream) == -1 && errno == EFAULT);
        Require(*emptyLinePointer == nullptr && *emptyCapacity == 0);
        Require(PageProtect(pointerStorage, cellPage, true));
        Require(fclose_nid_postfix(eofStream) == 0);
        Require(std::filesystem::remove(emptyPath));
        PageFree(pointerStorage);
        PageFree(capacityStorage);
    }

    free_nid_postfix(line);
    Require(std::filesystem::remove(path));
    Require(std::filesystem::remove(nulPath));
    return 0;
}
