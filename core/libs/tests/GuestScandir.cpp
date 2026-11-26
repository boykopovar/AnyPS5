#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <vector>

namespace {

void Require(bool condition) {
    if (!condition) std::abort();
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

int APS5_VABI KeepTextFiles(const GuestDirectoryEntry* entry) {
    errno = ENOENT;
    return std::strstr(entry->name, ".txt") != nullptr;
}

std::vector<const void*> g_compared;

int APS5_VABI CompareRecordingPlaces(const GuestDirectoryEntry* const* left, const GuestDirectoryEntry* const* right) {
    g_compared.push_back(left);
    g_compared.push_back(right);
    return std::strcmp((*left)->name, (*right)->name);
}

int APS5_VABI CompareAlwaysLess(const GuestDirectoryEntry* const*, const GuestDirectoryEntry* const*) {
    return -1;
}

int APS5_VABI CompareReverse(const GuestDirectoryEntry* const* left, const GuestDirectoryEntry* const* right) {
    return std::strcmp((*right)->name, (*left)->name);
}

}

int main(int argc, char** argv) {
    Require(argc == 2);
    RegisterAllocator();
    const std::filesystem::path base = argv[1];
    std::filesystem::remove_all(base);
    Require(std::filesystem::create_directory(base));
    for (const char* name : {"b.txt", "a.txt", "c.log"}) std::ofstream(base / name).put('x');

    GuestDirectoryEntry** list = nullptr;
    Require(scandir_nid_postfix(base.string().c_str(), &list, nullptr, alphasort_nid_postfix) == 5);
    Require(std::strcmp(list[0]->name, ".") == 0 && std::strcmp(list[2]->name, "a.txt") == 0);
    Require(std::strcmp(list[3]->name, "b.txt") == 0 && std::strcmp(list[4]->name, "c.log") == 0);
    for (int index = 0; index < 5; ++index) free_nid_postfix(list[index]);
    free_nid_postfix(list);

    list = nullptr;
    Require(scandir_nid_postfix(base.string().c_str(), &list, KeepTextFiles, alphasort_nid_postfix) == 2);
    Require(std::strcmp(list[0]->name, "a.txt") == 0 && std::strcmp(list[1]->name, "b.txt") == 0);
    for (int index = 0; index < 2; ++index) free_nid_postfix(list[index]);
    free_nid_postfix(list);

    list = nullptr;
    Require(scandir_nid_postfix(base.string().c_str(), &list, nullptr, CompareReverse) == 5);
    Require(std::strcmp(list[0]->name, "c.log") == 0 && std::strcmp(list[4]->name, ".") == 0);
    for (int index = 0; index < 5; ++index) free_nid_postfix(list[index]);
    free_nid_postfix(list);

    *__error_nid_postfix() = 0;
    list = nullptr;
    Require(scandir_nid_postfix((base / "absent").string().c_str(), &list, nullptr, alphasort_nid_postfix) == -1 && *__error_nid_postfix() == 2);
    Require(list == nullptr);

    const std::filesystem::path many = base / "many";
    Require(std::filesystem::create_directory(many));
    std::set<std::string> names{".", ".."};
    for (int index = 0; index < 40; ++index) {
        const std::string name = "f" + std::to_string(index);
        std::ofstream(many / name).put('x');
        names.insert(name);
    }
    const int total = static_cast<int>(names.size());

    list = nullptr;
    g_compared.clear();
    Require(scandir_nid_postfix(many.string().c_str(), &list, nullptr, CompareRecordingPlaces) == total);
    Require(!g_compared.empty());
    for (const void* place : g_compared) Require(place >= list && place < list + total);
    for (int index = 1; index < total; ++index) Require(std::strcmp(list[index - 1]->name, list[index]->name) < 0);
    for (int index = 0; index < total; ++index) free_nid_postfix(list[index]);
    free_nid_postfix(list);

    list = nullptr;
    Require(scandir_nid_postfix(many.string().c_str(), &list, nullptr, CompareAlwaysLess) == total);
    std::set<std::string> seen;
    for (int index = 0; index < total; ++index) {
        seen.insert(list[index]->name);
        free_nid_postfix(list[index]);
    }
    free_nid_postfix(list);
    Require(seen == names);

    std::filesystem::remove_all(base);
    return 0;
}
