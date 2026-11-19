#include "prx/libkernel_sys/include/AdditionalExports.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <set>
#include <string>
#include <unordered_map>

namespace {

constexpr int GuestEsrch = 3;
constexpr int GuestEinval = 22;
constexpr int GuestEagain = 35;
constexpr std::size_t NameCapacity = 32;
constexpr std::uint32_t FirstId = 1;
constexpr std::uint32_t LastId = 0xFFFF;
constexpr std::uint16_t TypeTag = 0x1000;

std::mutex namedObjectMutex;
std::unordered_map<std::uint32_t, std::uint16_t> namedObjectTypes;
std::set<std::uint32_t> releasedIds;
std::uint32_t nextUnusedId = FirstId;

int Fail(int error) {
    *__error_nid_postfix() = error;
    return -1;
}

}

extern "C" {

int APS5_VABI __sys_namedobj_create_nid_postfix(const char* name, void* object, std::uint32_t type) {
    static_cast<void>(object);
    if (name == nullptr) return Fail(GuestEinval);
    std::string copy;
    if (const int error = CopyGuestString_nid_no_patch(name, NameCapacity, copy)) return Fail(error);
    std::lock_guard lock(namedObjectMutex);
    const bool reuse = !releasedIds.empty();
    if (!reuse && nextUnusedId > LastId) return Fail(GuestEagain);
    const std::uint32_t id = reuse ? *releasedIds.begin() : nextUnusedId;
    namedObjectTypes.emplace(id, static_cast<std::uint16_t>(type));
    if (reuse) releasedIds.erase(releasedIds.begin());
    else ++nextUnusedId;
    return static_cast<int>(id);
}

int APS5_VABI __sys_namedobj_delete_nid_postfix(std::uint32_t id, std::uint32_t type) {
    std::lock_guard lock(namedObjectMutex);
    const auto found = namedObjectTypes.find(id);
    if (found == namedObjectTypes.end()) return Fail(GuestEsrch);
    if ((found->second | TypeTag) != (static_cast<std::uint16_t>(type) | TypeTag)) return Fail(GuestEsrch);
    releasedIds.insert(id);
    namedObjectTypes.erase(found);
    return 0;
}

}
