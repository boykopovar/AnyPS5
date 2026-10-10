#include "prx/libc/include/exceptions/Runtime.hpp"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <regex>
#include <windows.h>

using GuestWhat = const char* (APS5_VABI *)(const void*);
using GuestDestroy = void (APS5_VABI *)(void*);
using HostDestroy = void (*)(void*);
using GuestThrowFunction = void (APS5_VABI *)(std::uintptr_t);
struct TypeRecord { const void* vtable; const char* name; const TypeRecord* base; };
struct Object { const void* vtable; const char* message; };
struct Table { std::ptrdiff_t offset; const TypeRecord* type; GuestDestroy destroy; GuestDestroy deleteObject; GuestWhat what; };
struct Message { std::size_t length; std::size_t capacity; std::atomic<std::ptrdiff_t> references; };
static_assert(sizeof(Object) == 16);

extern "C" {
extern const unsigned char _ZTVN10__cxxabiv117__class_type_infoE_nid_postfix[];
extern const unsigned char _ZTVN10__cxxabiv120__si_class_type_infoE_nid_postfix[];
extern const unsigned char _ZTISt12out_of_range_nid_postfix[];
extern const unsigned char _ZTISt11regex_error_nid_postfix[];
void APS5_VABI _ZNSt12out_of_rangeC1EPKc_nid_postfix(Object*, const char*);
void APS5_VABI _ZNSt12out_of_rangeC1ERKS__nid_postfix(Object*, const Object*);
void APS5_VABI _ZNSt12out_of_rangeD1Ev_nid_postfix(Object*);
[[noreturn]] void APS5_VABI _ZSt14_Xout_of_rangePKc_nid_postfix(const char*);
[[noreturn]] void APS5_VABI _ZSt13_Xregex_errorNSt15regex_constants10error_typeE_nid_postfix(std::regex_constants::error_type);
int APS5_VABI GuestOuter();
void APS5_VABI GuestStdCapture();
void APS5_VABI ProbeDestroy(GuestDestroy, void*, void*);
const char* APS5_VABI ProbeWhat(GuestWhat, const void*, const void*);
void APS5_VABI _ZNSt8bad_castC1Ev_nid_postfix(Object*);
void APS5_VABI _ZNSt9bad_allocC1Ev_nid_postfix(Object*);

TypeRecord FixtureBaseType{_ZTVN10__cxxabiv117__class_type_infoE_nid_postfix + 16, "11FixtureBase", nullptr};
TypeRecord FixtureErrorType{_ZTVN10__cxxabiv120__si_class_type_infoE_nid_postfix + 16, "12FixtureError", &FixtureBaseType};
TypeRecord FixtureOtherType{_ZTVN10__cxxabiv117__class_type_infoE_nid_postfix + 16, "12FixtureOther", nullptr};
const void* FixtureStdOutType = _ZTISt12out_of_range_nid_postfix;
GuestThrowFunction FixtureThrow = reinterpret_cast<GuestThrowFunction>(_ZSt14_Xout_of_rangePKc_nid_postfix);
std::uintptr_t FixtureArgument;
extern const char FixtureMessage[] = "own System-V guest message";
void* FixtureLastObject = nullptr;
unsigned FixtureDestroyed = 0;
unsigned FixtureGuardDestroyed = 0;
unsigned FixtureCaught = 0;
unsigned FixtureWhatChecked = 0;

[[noreturn]] void APS5_VABI FixtureFail(unsigned code) {
    std::fprintf(stderr, "guest exception failure %u\n", code);
    ExitProcess(code);
}
const char* APS5_VABI FixtureWhat(const void* object) { return static_cast<const Object*>(object)->message; }
void APS5_VABI FixtureDestroy(void* object) {
    if (object != FixtureLastObject) FixtureFail(11);
    ++FixtureDestroyed;
}
Table FixtureVtable{0, &FixtureErrorType, FixtureDestroy, FixtureDestroy, FixtureWhat};
void APS5_VABI FixtureCheckWhat(const char* message) {
    if (!message || std::strcmp(message, FixtureMessage)) FixtureFail(12);
    ++FixtureWhatChecked;
}
}

static Object decoy{}, retained{};
static GuestDestroy registeredDestructor;
static void* expectedObject;
static void* poison;
static unsigned callbacks;
static bool regularRelease;
static bool keepMessage;
static const char* expectedMessage = FixtureMessage;

using NativeFree = void (*)(void*);
static NativeFree originalFree;
static std::uintptr_t* freeSlot;
static void* watchedObject;
static void* watchedMessage;
static void* foreignObject;
static void* foreignMessage;
static unsigned objectFrees, messageFrees;

static void TrackedFree(void* pointer) {
    if (pointer && (pointer == foreignObject || pointer == foreignMessage)) FixtureFail(50);
    if (pointer && pointer == watchedObject && ++objectFrees != 1) FixtureFail(51);
    if (pointer && pointer == watchedMessage && ++messageFrees != 1) FixtureFail(52);
    originalFree(pointer);
}

static void ReplaceFree(std::uintptr_t address) {
    DWORD protection;
    if (!VirtualProtect(freeSlot, sizeof(*freeSlot), PAGE_READWRITE, &protection)) FixtureFail(53);
    *freeSlot = address;
    DWORD ignored;
    if (!VirtualProtect(freeSlot, sizeof(*freeSlot), protection, &ignored)) FixtureFail(54);
}

static void TrackFree() {
    auto* image = reinterpret_cast<unsigned char*>(GetModuleHandleW(L"libc.prx"));
    if (!image) FixtureFail(55);
    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(image);
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS64*>(image + dos->e_lfanew);
    auto* imports = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(image +
        nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
    for (; imports->Name; ++imports) {
        if (!imports->OriginalFirstThunk) continue;
        auto* names = reinterpret_cast<IMAGE_THUNK_DATA64*>(image + imports->OriginalFirstThunk);
        auto* slots = reinterpret_cast<IMAGE_THUNK_DATA64*>(image + imports->FirstThunk);
        for (; names->u1.AddressOfData; ++names, ++slots) {
            if (IMAGE_SNAP_BY_ORDINAL64(names->u1.Ordinal)) continue;
            auto* name = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(image + names->u1.AddressOfData);
            if (std::strcmp(name->Name, "free")) continue;
            freeSlot = reinterpret_cast<std::uintptr_t*>(&slots->u1.Function);
            originalFree = reinterpret_cast<NativeFree>(*freeSlot);
            ReplaceFree(reinterpret_cast<std::uintptr_t>(TrackedFree));
            return;
        }
    }
    FixtureFail(56);
}

static int TestVtable(const char* mode) {
    const bool plain = std::strstr(mode, "plain") != nullptr;
    const bool deleting = std::strstr(mode, "delete") != nullptr;
    const bool destroying = deleting || std::strstr(mode, "destroy") != nullptr;
    auto* actual = static_cast<Object*>(std::malloc(sizeof(Object)));
    auto* other = static_cast<Object*>(std::malloc(sizeof(Object)));
    if (!actual || !other) FixtureFail(57);
    actual->message = other->message = nullptr;
    if (plain) {
        _ZNSt9bad_allocC1Ev_nid_postfix(actual);
        _ZNSt8bad_castC1Ev_nid_postfix(other);
    } else {
        _ZNSt12out_of_rangeC1EPKc_nid_postfix(actual, "actual virtual message");
        _ZNSt12out_of_rangeC1EPKc_nid_postfix(other, "unrelated virtual message");
    }
    const Object originalOther = *other;
    watchedObject = actual;
    watchedMessage = plain ? nullptr : reinterpret_cast<Message*>(const_cast<char*>(actual->message)) - 1;
    foreignObject = other;
    foreignMessage = plain ? nullptr : reinterpret_cast<Message*>(const_cast<char*>(other->message)) - 1;
    TrackFree();
    auto* unused = std::strstr(mode, "zero") ? nullptr : other;
    GuestDestroy destroy, deleteObject;
    GuestWhat what;
    auto* slots = static_cast<const unsigned char*>(actual->vtable);
    std::memcpy(&destroy, slots, sizeof(destroy));
    std::memcpy(&deleteObject, slots + sizeof(void*), sizeof(deleteObject));
    std::memcpy(&what, slots + 2 * sizeof(void*), sizeof(what));
    if (destroying) {
        ProbeDestroy(deleting ? deleteObject : destroy, actual, unused);
        if (objectFrees != (deleting ? 1u : 0u) || messageFrees != (plain ? 0u : 1u)) FixtureFail(58);
        if (!deleting && actual->message) FixtureFail(59);
    } else {
        const char* result = ProbeWhat(what, actual, unused);
        if (!result || std::strcmp(result, plain ? "std::bad_alloc" : "actual virtual message") ||
            objectFrees || messageFrees) FixtureFail(60);
    }
    if (other->vtable != originalOther.vtable || other->message != originalOther.message ||
        (!plain && std::strcmp(other->message, "unrelated virtual message"))) FixtureFail(61);
    ReplaceFree(reinterpret_cast<std::uintptr_t>(originalFree));
    if (!plain) {
        if (!deleting) _ZNSt12out_of_rangeD1Ev_nid_postfix(actual);
        _ZNSt12out_of_rangeD1Ev_nid_postfix(other);
    }
    if (!deleting) std::free(actual);
    std::free(other);
    return 0;
}

static Message* MessageHeader(const Object& object) {
    return reinterpret_cast<Message*>(const_cast<char*>(object.message)) - 1;
}

extern "C" void APS5_VABI ObserveDestroy(void* pointer) {
    if (pointer != expectedObject || ++callbacks != 1) FixtureFail(40);
    ProbeDestroy(registeredDestructor, pointer, poison);
    if (static_cast<Object*>(pointer)->message != nullptr ||
        !decoy.message || std::strcmp(decoy.message, "unrelated message") ||
        (keepMessage && MessageHeader(retained)->references.load() != 0)) FixtureFail(41);
}

extern "C" void APS5_VABI FixtureInspect(void* pointer) {
    auto* header = LibcException::FromObject(pointer);
    if (header->_pad != 0 || !header->destructor) FixtureFail(42);
    expectedObject = pointer;
    GuestWhat what;
    std::memcpy(&what, static_cast<const unsigned char*>(static_cast<Object*>(pointer)->vtable) +
        2 * sizeof(void*), sizeof(what));
    const char* message = ProbeWhat(what, pointer, poison);
    if (!message || std::strcmp(message, expectedMessage)) FixtureFail(62);
    if (keepMessage) {
        _ZNSt12out_of_rangeC1ERKS__nid_postfix(&retained, static_cast<Object*>(pointer));
        if (MessageHeader(retained)->references.load() != 1) FixtureFail(43);
    }
    registeredDestructor = reinterpret_cast<GuestDestroy>(header->destructor);
    if (!regularRelease) header->destructor = reinterpret_cast<HostDestroy>(ObserveDestroy);
}

int main(int argc, char** argv) {
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    if (argc != 2) return 2;
    if (std::strncmp(argv[1], "vtable-", 7) == 0) return TestVtable(argv[1]);
    if (std::strcmp(argv[1], "control") == 0) {
        if (GuestOuter() != 0 || FixtureDestroyed != 1 || FixtureGuardDestroyed != 1 ||
            FixtureCaught != 2 || FixtureWhatChecked != 2) return 10;
        return 0;
    }
    const bool regex = std::strstr(argv[1], "regex") != nullptr;
    regularRelease = std::strcmp(argv[1], "release") == 0;
    keepMessage = std::strstr(argv[1], "unshared") == nullptr;
    FixtureArgument = reinterpret_cast<std::uintptr_t>(FixtureMessage);
    if (regex) {
        expectedMessage = "regular expression error";
        FixtureStdOutType = _ZTISt11regex_error_nid_postfix;
        FixtureThrow = reinterpret_cast<GuestThrowFunction>(_ZSt13_Xregex_errorNSt15regex_constants10error_typeE_nid_postfix);
        FixtureArgument = static_cast<std::uintptr_t>(std::regex_constants::error_collate);
    }
    _ZNSt12out_of_rangeC1EPKc_nid_postfix(&decoy, "unrelated message");
    poison = std::strstr(argv[1], "zero") ? nullptr : &decoy;
    GuestStdCapture();
    if (callbacks != (regularRelease ? 0u : 1u) ||
        (keepMessage && (MessageHeader(retained)->references.load() != 0 ||
            std::strcmp(retained.message, regex ? "regular expression error" : FixtureMessage))) ||
        !decoy.message || std::strcmp(decoy.message, "unrelated message")) return 44;
    _ZNSt12out_of_rangeD1Ev_nid_postfix(&retained);
    _ZNSt12out_of_rangeD1Ev_nid_postfix(&decoy);
    if (retained.message || decoy.message) return 45;
    std::puts("guest message callback, retained ownership and release: PASS");
    return 0;
}
