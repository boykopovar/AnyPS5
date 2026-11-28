#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libSceLibreSsl/include/SslInternal.hpp"
#include "prx/libc/include/Api.hpp"
#include <array>
#include <cstdint>
#include <mutex>
#include <new>
#include <stdexcept>
#include <vector>

namespace {
constexpr int CryptoLibrary = 15;
constexpr int AnyFunction = 0xfff;
constexpr int MallocFailure = 65;
constexpr int SslClass = 1;
constexpr int SslContextClass = 2;
constexpr int StoreContextClass = 5;
constexpr int ClassCount = 16;

struct ExDataFunctions {
    std::int64_t argl;
    void* argp;
    LibreSslExNew newFunction;
    LibreSslExDup dupFunction;
    LibreSslExFree freeFunction;
};

std::mutex classesLock;
std::array<std::vector<ExDataFunctions>, ClassCount> classes;
std::mutex storeContextLock;
int storeContextIndex = -1;

int NewIndex(int classIndex, const ExDataFunctions& functions) {
    std::lock_guard lock(classesLock);
    auto& registered = classes[classIndex];
    try {
        registered.push_back(functions);
    } catch (const std::bad_alloc&) {
        LibreSslPutError_nid_no_patch(CryptoLibrary, AnyFunction, MallocFailure);
        return -1;
    }
    return static_cast<int>(registered.size() - 1);
}

std::vector<ExDataFunctions> Registered(int classIndex) {
    if (classIndex < 0 || classIndex >= ClassCount) throw std::invalid_argument("ex_data class index out of range");
    std::lock_guard lock(classesLock);
    return classes[classIndex];
}

std::size_t RegisteredCount(int classIndex) {
    if (classIndex < 0 || classIndex >= ClassCount) throw std::invalid_argument("ex_data class index out of range");
    std::lock_guard lock(classesLock);
    return classes[classIndex].size();
}

bool RegisteredAt(int classIndex, std::size_t index, ExDataFunctions& functions) {
    std::lock_guard lock(classesLock);
    const auto& registered = classes[classIndex];
    if (index >= registered.size()) return false;
    functions = registered[index];
    return true;
}

void* Stored(const LibreSslExData* data, int index) {
    const auto* stack = static_cast<const LibreSslStack*>(data->stack);
    if (!stack || index < 0 || index >= stack->num) return nullptr;
    return stack->data[index];
}
}

extern "C" {

int APS5_VABI SSL_get_ex_new_index_nid_postfix(std::int64_t argl, void* argp, LibreSslExNew newFunction, LibreSslExDup dupFunction, LibreSslExFree freeFunction) {
    return NewIndex(SslClass, {argl, argp, newFunction, dupFunction, freeFunction});
}

int APS5_VABI SSL_CTX_get_ex_new_index_nid_postfix(std::int64_t argl, void* argp, LibreSslExNew newFunction, LibreSslExDup dupFunction, LibreSslExFree freeFunction) {
    return NewIndex(SslContextClass, {argl, argp, newFunction, dupFunction, freeFunction});
}

int APS5_VABI SSL_get_ex_data_X509_STORE_CTX_idx_nid_postfix(void) {
    static char verifyCallback[] = "SSL for verify callback";
    std::lock_guard lock(storeContextLock);
    if (storeContextIndex < 0) storeContextIndex = NewIndex(StoreContextClass, {0, verifyCallback, nullptr, nullptr, nullptr});
    return storeContextIndex;
}

void APS5_VABI CRYPTO_cleanup_all_ex_data_nid_postfix(void) {
    std::lock_guard lock(classesLock);
    for (auto& registered : classes) std::vector<ExDataFunctions>().swap(registered);
}

bool LibreSslNewExData_nid_no_patch(int classIndex, void* object, LibreSslExData* data) {
    data->stack = nullptr;
    std::vector<ExDataFunctions> registered;
    try {
        registered = Registered(classIndex);
    } catch (const std::bad_alloc&) {
        LibreSslPutError_nid_no_patch(CryptoLibrary, AnyFunction, MallocFailure);
        return false;
    }
    for (std::size_t index = 0; index < registered.size(); ++index) {
        const ExDataFunctions& functions = registered[index];
        if (functions.newFunction) functions.newFunction(object, Stored(data, static_cast<int>(index)), data, static_cast<int>(index), functions.argl, functions.argp);
    }
    return true;
}

void LibreSslFreeExData_nid_no_patch(int classIndex, void* object, LibreSslExData* data) {
    const std::size_t count = RegisteredCount(classIndex);
    for (std::size_t index = 0; index < count; ++index) {
        ExDataFunctions functions;
        if (!RegisteredAt(classIndex, index, functions)) break;
        if (functions.freeFunction) functions.freeFunction(object, Stored(data, static_cast<int>(index)), data, static_cast<int>(index), functions.argl, functions.argp);
    }
    auto* stack = static_cast<LibreSslStack*>(data->stack);
    if (stack) {
        free_nid_postfix(stack->data);
        free_nid_postfix(stack);
        data->stack = nullptr;
    }
}

}
