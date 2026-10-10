#include "prx/libc/include/exceptions/Runtime.hpp"

#include <Testing/Test.hpp>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <regex>
#include <string>
#include <string_view>
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
    std::fprintf(stderr, "guest exception failure %u inside a guest frame\n", code);
    std::fflush(stderr);
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

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

constexpr std::array<std::string_view, 9> captureModes{
    "decoy", "zero", "release", "regex-decoy", "regex-zero",
    "unshared-decoy", "unshared-zero", "regex-unshared-decoy", "regex-unshared-zero"};

Object decoy{};
Object retained{};
GuestDestroy registeredDestructor;
void* expectedObject;
void* poison;
unsigned callbacks;
bool regularRelease;
bool keepMessage;
const char* expectedMessage = FixtureMessage;

using NativeFree = void (*)(void*);
NativeFree originalFree;
std::uintptr_t* freeSlot;
void* watchedObject;
void* watchedMessage;
void* foreignObject;
void* foreignMessage;
unsigned objectFrees;
unsigned messageFrees;

std::string_view SelectedMode() {
    const auto& arguments = Testing::Arguments();
    return arguments.empty() ? std::string_view{} : std::string_view(arguments.front());
}

bool Contains(std::string_view text, std::string_view part) {
    return text.find(part) != std::string_view::npos;
}

void TrackedFree(void* pointer) {
    if (pointer && (pointer == foreignObject || pointer == foreignMessage)) FixtureFail(50);
    if (pointer && pointer == watchedObject && ++objectFrees != 1) FixtureFail(51);
    if (pointer && pointer == watchedMessage && ++messageFrees != 1) FixtureFail(52);
    originalFree(pointer);
}

void ReplaceFree(std::uintptr_t address) {
    DWORD protection;
    Require(VirtualProtect(freeSlot, sizeof(*freeSlot), PAGE_READWRITE, &protection) != 0, "unprotect the free import slot");
    *freeSlot = address;
    DWORD ignored;
    Require(VirtualProtect(freeSlot, sizeof(*freeSlot), protection, &ignored) != 0, "reprotect the free import slot");
}

std::uintptr_t* FindFreeImport() {
    auto* image = reinterpret_cast<unsigned char*>(GetModuleHandleW(L"libc.prx"));
    Require(image != nullptr, "libc.prx is loaded");
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
            return reinterpret_cast<std::uintptr_t*>(&slots->u1.Function);
        }
    }
    Testing::Fail("libc.prx imports free");
}

class FreeTracker {
public:
    FreeTracker() {
        freeSlot = FindFreeImport();
        originalFree = reinterpret_cast<NativeFree>(*freeSlot);
        ReplaceFree(reinterpret_cast<std::uintptr_t>(TrackedFree));
        active = true;
    }

    ~FreeTracker() {
        if (active) Restore();
    }

    FreeTracker(const FreeTracker&) = delete;
    FreeTracker& operator=(const FreeTracker&) = delete;

    void Restore() {
        active = false;
        DWORD protection;
        if (VirtualProtect(freeSlot, sizeof(*freeSlot), PAGE_READWRITE, &protection)) {
            *freeSlot = reinterpret_cast<std::uintptr_t>(originalFree);
            DWORD ignored;
            VirtualProtect(freeSlot, sizeof(*freeSlot), protection, &ignored);
        }
    }

private:
    bool active = false;
};

Message* MessageHeader(const Object& object) {
    return reinterpret_cast<Message*>(const_cast<char*>(object.message)) - 1;
}

void SuppressErrorDialogs() {
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
}

} // namespace

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

namespace {

const Case control{"GuestException_ThrowCatchRethrow_DestroysOnceAndChecksWhat", [] {
    if (SelectedMode() != "control") Testing::Skip("needs its own process, selected by the argument 'control'");
    SuppressErrorDialogs();
    RequireEqual(GuestOuter(), 0, "guest outer frame result");
    RequireEqual(FixtureDestroyed, 1u, "exception object destructions");
    RequireEqual(FixtureGuardDestroyed, 1u, "cleanup landing pads run");
    RequireEqual(FixtureCaught, 2u, "catch handlers entered");
    RequireEqual(FixtureWhatChecked, 2u, "what() checks in guest handlers");
}};

const Case capture{"GuestStdException_CaughtWithPoisonedRegisters_RetainsAndReleasesMessageOnce", [] {
    const std::string_view mode = SelectedMode();
    bool known = false;
    for (const auto candidate : captureModes) known = known || candidate == mode;
    if (!known) Testing::Skip("needs its own process, selected by a capture mode argument such as 'decoy'");
    SuppressErrorDialogs();
    const bool regex = Contains(mode, "regex");
    regularRelease = mode == "release";
    keepMessage = !Contains(mode, "unshared");
    FixtureArgument = reinterpret_cast<std::uintptr_t>(FixtureMessage);
    if (regex) {
        expectedMessage = "regular expression error";
        FixtureStdOutType = _ZTISt11regex_error_nid_postfix;
        FixtureThrow = reinterpret_cast<GuestThrowFunction>(_ZSt13_Xregex_errorNSt15regex_constants10error_typeE_nid_postfix);
        FixtureArgument = static_cast<std::uintptr_t>(std::regex_constants::error_collate);
    }
    _ZNSt12out_of_rangeC1EPKc_nid_postfix(&decoy, "unrelated message");
    poison = Contains(mode, "zero") ? nullptr : &decoy;
    GuestStdCapture();
    RequireEqual(callbacks, regularRelease ? 0u : 1u, "observed destructor callbacks");
    if (keepMessage) {
        RequireEqual(MessageHeader(retained)->references.load(), std::ptrdiff_t{0}, "retained message references");
        RequireEqual(std::string_view(retained.message), std::string_view(regex ? "regular expression error" : FixtureMessage),
            "retained message");
    }
    Require(decoy.message != nullptr, "unrelated exception keeps its message");
    RequireEqual(std::string_view(decoy.message), std::string_view("unrelated message"), "unrelated exception message");
    _ZNSt12out_of_rangeD1Ev_nid_postfix(&retained);
    _ZNSt12out_of_rangeD1Ev_nid_postfix(&decoy);
    Require(retained.message == nullptr, "destroying the retained copy clears its message");
    Require(decoy.message == nullptr, "destroying the unrelated exception clears its message");
}};

const Case vtable{"GuestStdException_VirtualSlotsWithPoisonedRegisters_TouchOnlyTheirObject", [] {
    const std::string_view mode = SelectedMode();
    if (!mode.starts_with("vtable-")) Testing::Skip("needs its own process, selected by a 'vtable-*' argument");
    SuppressErrorDialogs();
    const bool plain = Contains(mode, "plain");
    const bool deleting = Contains(mode, "delete");
    const bool destroying = deleting || Contains(mode, "destroy");
    auto* actual = static_cast<Object*>(std::malloc(sizeof(Object)));
    auto* other = static_cast<Object*>(std::malloc(sizeof(Object)));
    Require(actual != nullptr && other != nullptr, "allocate exception objects");
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
    FreeTracker tracker;
    auto* unused = Contains(mode, "zero") ? nullptr : other;
    GuestDestroy destroy;
    GuestDestroy deleteObject;
    GuestWhat what;
    auto* slots = static_cast<const unsigned char*>(actual->vtable);
    std::memcpy(&destroy, slots, sizeof(destroy));
    std::memcpy(&deleteObject, slots + sizeof(void*), sizeof(deleteObject));
    std::memcpy(&what, slots + 2 * sizeof(void*), sizeof(what));
    if (destroying) {
        ProbeDestroy(deleting ? deleteObject : destroy, actual, unused);
        RequireEqual(objectFrees, deleting ? 1u : 0u, "object frees");
        RequireEqual(messageFrees, plain ? 0u : 1u, "message frees");
        if (!deleting) Require(actual->message == nullptr, "destroying clears the message");
    } else {
        const char* result = ProbeWhat(what, actual, unused);
        Require(result != nullptr, "what() result");
        RequireEqual(std::string_view(result), std::string_view(plain ? "std::bad_alloc" : "actual virtual message"), "what()");
        RequireEqual(objectFrees, 0u, "object frees");
        RequireEqual(messageFrees, 0u, "message frees");
    }
    Require(other->vtable == originalOther.vtable, "unrelated object keeps its vtable");
    Require(other->message == originalOther.message, "unrelated object keeps its message");
    if (!plain) {
        RequireEqual(std::string_view(other->message), std::string_view("unrelated virtual message"), "unrelated message text");
    }
    tracker.Restore();
    if (!plain) {
        if (!deleting) _ZNSt12out_of_rangeD1Ev_nid_postfix(actual);
        _ZNSt12out_of_rangeD1Ev_nid_postfix(other);
    }
    if (!deleting) std::free(actual);
    std::free(other);
}};

} // namespace
