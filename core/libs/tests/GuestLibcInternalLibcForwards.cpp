#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libSceLibcInternal/include/RecoveredInventory.hpp"
#include "prx/libkernel/Pthread/include/Mutex.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

struct TypeRecord { const void* vtable; const char* name; const TypeRecord* base; };

using GuestWhat = const char* (APS5_VABI *)(const LibcException::ExceptionObject*);
using GuestName = const char* (APS5_VABI *)(const LibcException::CategoryObject*);
using Thrower = void (APS5_VABI *)(const void*);

extern "C" {
extern const TypeRecord _ZTISt9exception_nid_postfix;
extern const TypeRecord _ZTISt11logic_error_nid_postfix;
extern const TypeRecord _ZTISt13runtime_error_nid_postfix;
extern const TypeRecord _ZTISt14overflow_error_nid_postfix;

void APS5_VABI SceV2Catch(Thrower thrower, const void* argument);

const void* SceV2CatchType = nullptr;
const void* SceV2OtherType = nullptr;
int SceV2Selector = 0;
}

namespace {

unsigned failures = 0;
unsigned releases = 0;
void* lastReleased = nullptr;
std::string caughtType;
std::string caughtWhat;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "FAIL: %s\n", message);
    ++failures;
}

void* APS5_VABI TestAllocate(std::size_t size) { return std::malloc(size); }
void APS5_VABI TestFree(void* pointer) {
    ++releases;
    lastReleased = pointer;
    std::free(pointer);
}
void* APS5_VABI TestCalloc(std::size_t count, std::size_t size) { return std::calloc(count, size); }
void* APS5_VABI TestReallocate(void* pointer, std::size_t size) { return std::realloc(pointer, size); }
void* APS5_VABI TestAlign(std::size_t, std::size_t size) { return std::malloc(size); }
void* APS5_VABI TestRealign(void* pointer, std::size_t size, std::size_t) { return std::realloc(pointer, size); }
int APS5_VABI TestPosixAlign(void** pointer, std::size_t, std::size_t size) {
    *pointer = std::malloc(size);
    return *pointer ? 0 : 12;
}

using Destructor = void (APS5_VABI*)(void*);

const char* CategoryName(const LibcException::CategoryObject* category) {
    const auto* vtable = static_cast<const void* const*>(category->vtable);
    return reinterpret_cast<GuestName>(const_cast<void*>(vtable[2]))(category);
}

void CheckDeletingDestructor(Destructor destructor, const LibcException::CategoryObject* singleton, const char* label) {
    auto* object = static_cast<LibcException::CategoryObject*>(_Znwm_nid_postfix(sizeof(LibcException::CategoryObject)));
    object->vtable = singleton->vtable;
    releases = 0;
    lastReleased = nullptr;
    destructor(object);
    Require(releases == 1 && lastReleased == object, label);
}

void CheckCompleteDestructor(Destructor destructor, const LibcException::CategoryObject* singleton, const char* name, const char* label) {
    LibcException::CategoryObject object{singleton->vtable};
    releases = 0;
    destructor(&object);
    Require(releases == 0 && object.vtable == singleton->vtable, label);
    destructor(const_cast<LibcException::CategoryObject*>(singleton));
    Require(releases == 0 && std::strcmp(CategoryName(singleton), name) == 0, label);
}

void CheckCategories() {
    CheckDeletingDestructor(&sceLibcInternalFutureErrorCategoryDeletingDestructor_nid_no_patch,
        _ZSt15future_categoryv_nid_postfix(), "_Future_error_category D0 releases the object");
    CheckDeletingDestructor(&sceLibcInternalSystemErrorCategoryDeletingDestructor_nid_no_patch,
        _ZSt15system_categoryv_nid_postfix(), "_System_error_category D0 releases the object");
    CheckCompleteDestructor(&sceLibcInternalSystemErrorCategoryDestructor_nid_no_patch,
        _ZSt15system_categoryv_nid_postfix(), "system", "_System_error_category D1 frees nothing");
    CheckCompleteDestructor(&sceLibcInternalGenericErrorCategoryDestructor_nid_no_patch,
        _ZSt16generic_categoryv_nid_postfix(), "generic", "_Generic_error_category D1 frees nothing");
}

TypeRecord Named(const char* name) {
    return {_ZTISt9exception_nid_postfix.vtable, name, nullptr};
}

void APS5_VABI ThrowOverflow(const void* message) {
    sceLibcInternalXoverflowError_nid_no_patch(static_cast<const char*>(message));
}

void CatchOverflow(const char* message, const void* catchType, const void* otherType, const char* expected, const char* label) {
    SceV2CatchType = catchType;
    SceV2OtherType = otherType;
    SceV2Selector = 0;
    caughtType.clear();
    caughtWhat.clear();
    SceV2Catch(ThrowOverflow, message);
    Require(SceV2Selector == 2, label);
    Require(caughtType == "St14overflow_error", label);
    Require(caughtWhat == expected, label);
}

void CheckOverflow() {
    const TypeRecord versioned = Named("NSt8__sce_v214overflow_errorE");
    CatchOverflow("value too large", &_ZTISt14overflow_error_nid_postfix, &versioned, "value too large",
        "_Xoverflow_error throws std::overflow_error, not the __sce_v2 type");
    CatchOverflow("runtime", &_ZTISt13runtime_error_nid_postfix, &_ZTISt11logic_error_nid_postfix, "runtime",
        "_Xoverflow_error is a runtime_error and not a logic_error");
    CatchOverflow(nullptr, &_ZTISt9exception_nid_postfix, &versioned, "", "_Xoverflow_error takes a null message");
}

constexpr int success = 0;
constexpr int busy = 3;
constexpr int plain = 0x01;
constexpr int recursive = 0x100;

int InitWithOverride(PthreadMutex* mutex, int type, const char* name) {
    return _ZNSt8__sce_v236_Mtx_init_with_default_name_overrideEPP13pthread_mutexiPKc_nid_postfix(reinterpret_cast<void**>(mutex), type, name);
}

void CheckMutex() {
    PthreadMutex named = reinterpret_cast<PthreadMutex>(std::uintptr_t{1});
    Require(InitWithOverride(&named, plain, "override") == success && named != nullptr, "the override creates a mutex");
    Require(_ZNSt8__sce_v29_Mtx_lockEPP13pthread_mutex_nid_postfix(reinterpret_cast<void**>(&named)) == success, "the mutex locks");
    Require(_ZNSt8__sce_v29_Mtx_lockEPP13pthread_mutex_nid_postfix(reinterpret_cast<void**>(&named)) == busy, "a plain mutex reports a relock as busy");
    Require(_ZNSt8__sce_v211_Mtx_unlockEPP13pthread_mutex_nid_postfix(reinterpret_cast<void**>(&named)) == success, "the mutex unlocks");
    Require(scePthreadMutexLock(&named) == 0, "the handle is a libkernel pthread mutex");
    Require(scePthreadMutexUnlock(&named) == 0, "libkernel unlocks the handle");
    Require(_ZNSt8__sce_v212_Mtx_destroyEPP13pthread_mutex_nid_postfix(reinterpret_cast<void**>(&named)) == success, "the mutex is destroyed");

    PthreadMutex nested = nullptr;
    Require(InitWithOverride(&nested, plain | recursive, nullptr) == success && nested != nullptr, "a null name creates a recursive mutex");
    Require(_ZNSt8__sce_v29_Mtx_lockEPP13pthread_mutex_nid_postfix(reinterpret_cast<void**>(&nested)) == success, "the recursive mutex locks");
    Require(_ZNSt8__sce_v212_Mtx_trylockEPP13pthread_mutex_nid_postfix(reinterpret_cast<void**>(&nested)) == success, "the recursive mutex relocks");
    Require(_ZNSt8__sce_v211_Mtx_unlockEPP13pthread_mutex_nid_postfix(reinterpret_cast<void**>(&nested)) == success, "the first unlock succeeds");
    Require(_ZNSt8__sce_v211_Mtx_unlockEPP13pthread_mutex_nid_postfix(reinterpret_cast<void**>(&nested)) == success, "the second unlock succeeds");
    Require(_ZNSt8__sce_v212_Mtx_destroyEPP13pthread_mutex_nid_postfix(reinterpret_cast<void**>(&nested)) == success, "the recursive mutex is destroyed");

    PthreadMutex empty = nullptr;
    Require(InitWithOverride(&empty, plain, "") == success && empty != nullptr, "an empty name creates a mutex");
    Require(_ZNSt8__sce_v212_Mtx_destroyEPP13pthread_mutex_nid_postfix(reinterpret_cast<void**>(&empty)) == success, "the unnamed mutex is destroyed");
}

}

extern "C" void APS5_VABI SceV2Inspect(void* object) {
    const auto* exception = static_cast<const LibcException::ExceptionObject*>(object);
    const auto* vtable = static_cast<const void* const*>(exception->vtable);
    caughtType = static_cast<const TypeRecord*>(vtable[-1])->name;
    caughtWhat = reinterpret_cast<GuestWhat>(const_cast<void*>(vtable[2]))(exception);
}

int main() {
    const std::array<void*, 10> api{
        reinterpret_cast<void*>(&TestAllocate), reinterpret_cast<void*>(&TestFree),
        reinterpret_cast<void*>(&TestCalloc), reinterpret_cast<void*>(&TestReallocate),
        reinterpret_cast<void*>(&TestAlign), reinterpret_cast<void*>(&TestRealign),
        reinterpret_cast<void*>(&TestPosixAlign), nullptr, nullptr, nullptr};
    ApplicationHeapRegister_nid_no_patch(api.data());

    CheckCategories();
    CheckOverflow();
    CheckMutex();

    if (failures != 0) return 1;
    std::puts("libc internal libc forward checks passed");
    return 0;
}
