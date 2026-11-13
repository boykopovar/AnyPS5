#include "prx/libSceNpParty/include/Api.hpp"
#include <atomic>
#include <cerrno>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <mutex>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int NP_PARTY_ERROR_ALREADY_INITIALIZED = static_cast<int>(0x80552502u);
constexpr int NP_PARTY_ERROR_NOT_INITIALIZED = static_cast<int>(0x80552503u);
constexpr int NP_PARTY_ERROR_INVALID_ARGUMENT = static_cast<int>(0x80552504u);
constexpr int NP_PARTY_ERROR_NOT_IN_PARTY = static_cast<int>(0x80552506u);
constexpr int NP_PARTY_ERROR_HANDLER_STATE = static_cast<int>(0x80552511u);
constexpr std::uint16_t NP_PARTY_STATE_NOT_IN_PARTY = 2;
constexpr std::uint32_t NP_PARTY_VERSION_LEGACY = 0x54fffff;
constexpr std::uint32_t NP_PARTY_VERSION_RESERVED = 0x1700000;

std::mutex g_handlerMutex;
std::atomic<bool> g_initialized{false};
std::atomic<std::uint32_t> g_version{0};
std::atomic<void*> g_handlerCallback{nullptr};
std::atomic<void*> g_privateHandler{nullptr};
std::atomic<bool> g_handlerRegistered{false};

template <typename T>
bool ReadGuest(const void* base, std::size_t offset, T& value) {
    const int savedErrno = errno;
    if (!GuestCopyIn_nid_no_patch(&value, static_cast<const std::uint8_t*>(base) + offset, sizeof(value))) {
        errno = savedErrno;
        return false;
    }
    return true;
}

bool GuestRangeIsZero(const void* base, std::size_t offset, std::size_t bytes) {
    std::uint8_t buffer[0x200];
    if (bytes > sizeof(buffer)) return false;
    if (!GuestCopyIn_nid_no_patch(buffer, static_cast<const std::uint8_t*>(base) + offset, bytes)) return false;
    for (std::size_t i = 0; i < bytes; ++i) {
        if (buffer[i] != 0) return false;
    }
    return true;
}

bool GuestBlobIsReadable(std::uint64_t address, std::size_t bytes) {
    std::uint8_t buffer[0x800];
    if (bytes > sizeof(buffer)) return false;
    return GuestCopyIn_nid_no_patch(buffer, reinterpret_cast<const void*>(address), bytes);
}

bool EntriesAreUnique(std::uint64_t address, std::uint8_t count, std::size_t entrySize) {
    std::uint8_t entries[16][0x14] = {};
    for (std::uint8_t i = 0; i < count; ++i) {
        if (!GuestCopyIn_nid_no_patch(entries[i], reinterpret_cast<const void*>(address + static_cast<std::uint64_t>(i) * entrySize), entrySize)) return false;
    }
    for (std::uint8_t i = 0; i < count; ++i) {
        for (std::uint8_t j = static_cast<std::uint8_t>(i + 1); j < count; ++j) {
            if (std::memcmp(entries[i], entries[j], entrySize) == 0) return false;
        }
    }
    return true;
}

bool ValidateCreateParam(const void* param, bool variantA) {
    if (param == nullptr) return false;
    const std::uint32_t version = g_version.load();
    const std::size_t entrySize = variantA ? 8 : 0x14;
    const std::size_t arrayOffset = variantA ? 0xC0 : 0x2C0;
    const std::size_t countOffset = variantA ? 0xC8 : 0x2C8;
    const std::size_t flagOffset = variantA ? 0xC9 : 0x2C9;
    const std::size_t dataOffset = variantA ? 0xD0 : 0x2D0;
    const std::size_t sizeOffset = variantA ? 0xD8 : 0x2D8;
    const std::size_t tailOffset = variantA ? 0xDA : 0x2DA;
    std::uint16_t dataSize = 0;
    std::uint8_t count = 0;
    std::uint8_t flag = 0;
    if (!ReadGuest(param, sizeOffset, dataSize) || dataSize > 0x800) return false;
    if (!ReadGuest(param, countOffset, count) || count > 0x10) return false;
    if (!ReadGuest(param, flagOffset, flag)) return false;
    if (count != 0) {
        std::uint64_t entries = 0;
        if (!ReadGuest(param, arrayOffset, entries) || entries == 0) return false;
        if (!EntriesAreUnique(entries, count, entrySize)) return false;
    }
    if (variantA) {
        if (count == 0 && flag != 0 && version > NP_PARTY_VERSION_LEGACY) return false;
        std::uint64_t data = 0;
        if (!ReadGuest(param, dataOffset, data)) return false;
        if (data != 0 && dataSize != 0 && !GuestBlobIsReadable(data, dataSize)) return false;
        if (!GuestRangeIsZero(param, 0x00, 0x80)) return false;
        if (!GuestRangeIsZero(param, 0x82, 0x3E)) return false;
        if (!GuestRangeIsZero(param, 0xCA, 6)) return false;
        if (!GuestRangeIsZero(param, tailOffset, 0x36)) return false;
    } else {
        if (version >= NP_PARTY_VERSION_RESERVED) {
            if (!GuestRangeIsZero(param, 0xC0, 0x200)) return false;
            if (count != 0) {
                std::uint64_t data = 0;
                if (!ReadGuest(param, dataOffset, data) || data == 0) return false;
                if (dataSize != 0 && !GuestBlobIsReadable(data, dataSize)) return false;
            }
        }
        if (count == 0) {
            if (!GuestRangeIsZero(param, 0x00, 0x80)) return false;
            if (!GuestRangeIsZero(param, 0x82, 0x3E)) return false;
            if (!GuestRangeIsZero(param, 0xCA, 6)) return false;
            if (!GuestRangeIsZero(param, tailOffset, 0x36)) return false;
        }
    }
    return true;
}

bool ValidateInvitationListParam(const void* param, bool variantA) {
    if (param == nullptr) return false;
    const std::size_t countOffset = variantA ? 0x8 : 0x208;
    const std::size_t arrayOffset = variantA ? 0x0 : 0x200;
    const std::size_t sizeOffset = variantA ? 0x18 : 0x218;
    const std::size_t entrySize = variantA ? 8 : 0x14;
    std::uint16_t dataSize = 0;
    std::uint8_t count = 0;
    if (!ReadGuest(param, sizeOffset, dataSize) || dataSize > 0x800) return false;
    if (!ReadGuest(param, countOffset, count) || count > 0x10) return false;
    if (count != 0) {
        std::uint64_t entries = 0;
        if (!ReadGuest(param, arrayOffset, entries) || entries == 0) return false;
        if (!EntriesAreUnique(entries, count, entrySize)) return false;
    }
    if (variantA) {
        std::uint8_t flag = 0;
        if (!ReadGuest(param, 0x9, flag)) return false;
        std::uint64_t data = 0;
        if (!ReadGuest(param, 0x10, data)) return false;
        if (data != 0 && dataSize != 0 && !GuestBlobIsReadable(data, dataSize)) return false;
        if (count == 0 && flag != 0 && g_version.load() > NP_PARTY_VERSION_LEGACY) return false;
        if (!GuestRangeIsZero(param, 0xA, 6)) return false;
        if (!GuestRangeIsZero(param, 0x1A, 0x36)) return false;
    } else {
        std::uint64_t data = 0;
        if (!ReadGuest(param, 0x210, data)) return false;
        if (data != 0 && dataSize != 0 && !GuestBlobIsReadable(data, dataSize)) return false;
        if (!GuestRangeIsZero(param, 0x20A, 6)) return false;
        if (!GuestRangeIsZero(param, 0x21A, 0x36)) return false;
        if (g_version.load() >= NP_PARTY_VERSION_RESERVED && !GuestRangeIsZero(param, 0x000, 0x200)) return false;
    }
    return true;
}

int notInParty(const void* out) {
    if (!g_initialized) return NP_PARTY_ERROR_NOT_INITIALIZED;
    if (!out) return NP_PARTY_ERROR_INVALID_ARGUMENT;
    return NP_PARTY_ERROR_NOT_IN_PARTY;
}

int RecordHandler(const void* handlers, bool rejectRegistered) {
    std::lock_guard lock(g_handlerMutex);
    if (!g_initialized) return NP_PARTY_ERROR_NOT_INITIALIZED;
    if (!handlers) return NP_PARTY_ERROR_INVALID_ARGUMENT;
    if (rejectRegistered && g_handlerRegistered.load()) return NP_PARTY_ERROR_HANDLER_STATE;
    const void* callback = nullptr;
    if (!ReadGuest(handlers, 8, callback)) return NP_PARTY_ERROR_INVALID_ARGUMENT;
    g_handlerCallback.store(const_cast<void*>(callback));
    g_handlerRegistered.store(true);
    return 0;
}

}

extern "C" {

int APS5_VABI sceNpPartyGetState(std::uint16_t* state) {
    if (!g_initialized) return NP_PARTY_ERROR_NOT_INITIALIZED;
    if (!state) return NP_PARTY_ERROR_INVALID_ARGUMENT;
    const std::uint16_t value = NP_PARTY_STATE_NOT_IN_PARTY;
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(state, &value, sizeof(value));
    errno = savedErrno;
    if (!copied) return NP_PARTY_ERROR_INVALID_ARGUMENT;
    return 0;
}

int APS5_VABI sceNpPartyCheckCallback(void) {
    if (!g_initialized) return NP_PARTY_ERROR_NOT_INITIALIZED;
    return 0;
}

int APS5_VABI sceNpPartyCreate(std::int32_t userId, const NpPartyCreateParam* param) {
    if (!g_initialized) return NP_PARTY_ERROR_NOT_INITIALIZED;
    if (userId == -1) return NP_PARTY_ERROR_INVALID_ARGUMENT;
    if (!ValidateCreateParam(param, false)) return NP_PARTY_ERROR_INVALID_ARGUMENT;
    return NP_PARTY_ERROR_NOT_IN_PARTY;
}

int APS5_VABI sceNpPartyCreateA(std::int32_t userId, const NpPartyCreateAParam* param) {
    if (!g_initialized) return NP_PARTY_ERROR_NOT_INITIALIZED;
    if (userId == -1) return NP_PARTY_ERROR_INVALID_ARGUMENT;
    if (!ValidateCreateParam(param, true)) return NP_PARTY_ERROR_INVALID_ARGUMENT;
    return NP_PARTY_ERROR_NOT_IN_PARTY;
}

int APS5_VABI sceNpPartyGetId(void* id) {
    return notInParty(id);
}

int APS5_VABI sceNpPartyGetMemberInfo(std::uint16_t, void* memberInfo) {
    return notInParty(memberInfo);
}

int APS5_VABI sceNpPartyGetMemberInfoA(std::uint16_t, void* memberInfo) {
    return notInParty(memberInfo);
}

int APS5_VABI sceNpPartyGetMemberSessionInfo(std::uint16_t, void* memberGameSessionInfo) {
    return notInParty(memberGameSessionInfo);
}

int APS5_VABI sceNpPartyGetMemberVoiceInfo(std::uint16_t, std::uint16_t, std::uint8_t* memberVoiceState) {
    return notInParty(memberVoiceState);
}

int APS5_VABI sceNpPartyGetMembers(void* memberList) {
    return notInParty(memberList);
}

int APS5_VABI sceNpPartyGetMembersA(void* memberList) {
    return notInParty(memberList);
}

int APS5_VABI sceNpPartyGetStateAsUser(std::int32_t userId, std::uint16_t* state) {
    if (!g_initialized) return NP_PARTY_ERROR_NOT_INITIALIZED;
    if (userId == 0) return NP_PARTY_ERROR_INVALID_ARGUMENT;
    return sceNpPartyGetState(state);
}

int APS5_VABI sceNpPartyGetStateAsUserA(std::int32_t userId, std::uint16_t* state) {
    if (!g_initialized) return NP_PARTY_ERROR_NOT_INITIALIZED;
    if (userId == -1) return NP_PARTY_ERROR_INVALID_ARGUMENT;
    return sceNpPartyGetState(state);
}

int APS5_VABI sceNpPartyGetVoiceChatPriority(std::uint32_t* priority) {
    if (!g_initialized) return NP_PARTY_ERROR_NOT_INITIALIZED;
    if (!priority) return NP_PARTY_ERROR_INVALID_ARGUMENT;
    return NP_PARTY_ERROR_NOT_IN_PARTY;
}

int APS5_VABI sceNpPartyInitialize(const void* param) {
    std::lock_guard lock(g_handlerMutex);
    if (!param) return NP_PARTY_ERROR_INVALID_ARGUMENT;
    std::uint32_t version = 0;
    const int savedErrno = errno;
    if (!GuestCopyIn_nid_no_patch(&version, param, sizeof(version))) {
        errno = savedErrno;
        return NP_PARTY_ERROR_INVALID_ARGUMENT;
    }
    if (version == 0) return NP_PARTY_ERROR_INVALID_ARGUMENT;
    bool expected = false;
    if (!g_initialized.compare_exchange_strong(expected, true)) return NP_PARTY_ERROR_ALREADY_INITIALIZED;
    g_version.store(version);
    g_handlerCallback.store(nullptr);
    g_privateHandler.store(nullptr);
    g_handlerRegistered.store(false);
    return 0;
}

int APS5_VABI sceNpPartyJoin(std::int32_t userId, NpPartyJoinParam param) {
    if (!g_initialized) return NP_PARTY_ERROR_NOT_INITIALIZED;
    if (userId == -1) return NP_PARTY_ERROR_INVALID_ARGUMENT;
    if (param.data[0] == 0) return NP_PARTY_ERROR_INVALID_ARGUMENT;
    return NP_PARTY_ERROR_NOT_IN_PARTY;
}

int APS5_VABI sceNpPartyLeave(std::uint16_t userId, const NpPartyLeaveParam* param) {
    if (!g_initialized) return NP_PARTY_ERROR_NOT_INITIALIZED;
    if (userId == 0) return NP_PARTY_ERROR_INVALID_ARGUMENT;
    if (param != nullptr) {
        NpPartyLeaveParam local = {};
        const int savedErrno = errno;
        if (!GuestCopyIn_nid_no_patch(&local, param, sizeof(local))) {
            errno = savedErrno;
            return NP_PARTY_ERROR_INVALID_ARGUMENT;
        }
        if (local.size > 0x800) return NP_PARTY_ERROR_INVALID_ARGUMENT;
        if (local.data != nullptr && local.size != 0) {
            std::uint8_t buffer[0x800];
            if (!GuestCopyIn_nid_no_patch(buffer, local.data, local.size)) {
                errno = savedErrno;
                return NP_PARTY_ERROR_INVALID_ARGUMENT;
            }
        }
    }
    return NP_PARTY_ERROR_NOT_IN_PARTY;
}

int APS5_VABI sceNpPartyRegisterHandlerA(const void* handlers, void*) {
    return RecordHandler(handlers, true);
}

int APS5_VABI sceNpPartyRegisterHandler(const void* handlers, void*) {
    return RecordHandler(handlers, false);
}

int APS5_VABI sceNpPartyRegisterPrivateHandler(void* handler) {
    std::lock_guard lock(g_handlerMutex);
    if (!g_initialized) return NP_PARTY_ERROR_NOT_INITIALIZED;
    if (!g_handlerCallback.load()) return NP_PARTY_ERROR_HANDLER_STATE;
    g_privateHandler.store(handler);
    return 0;
}

int APS5_VABI sceNpPartySendBinaryMessage(const void* sendParams) {
    return notInParty(sendParams);
}

int APS5_VABI sceNpPartySetVoiceChatPriority(std::int32_t priority) {
    if (!g_initialized) return NP_PARTY_ERROR_NOT_INITIALIZED;
    if (static_cast<std::uint32_t>(priority) - 1u > 1u) return NP_PARTY_ERROR_INVALID_ARGUMENT;
    return NP_PARTY_ERROR_NOT_IN_PARTY;
}

int APS5_VABI sceNpPartyShowInvitationList(std::uint16_t userId, const NpPartyShowInvitationListParam* param) {
    if (!g_initialized) return NP_PARTY_ERROR_NOT_INITIALIZED;
    if (userId == 0) return NP_PARTY_ERROR_INVALID_ARGUMENT;
    if (!ValidateInvitationListParam(param, false)) return NP_PARTY_ERROR_INVALID_ARGUMENT;
    return NP_PARTY_ERROR_NOT_IN_PARTY;
}

int APS5_VABI sceNpPartyShowInvitationListA(std::uint16_t userId, const NpPartyShowInvitationListAParam* param) {
    if (!g_initialized) return NP_PARTY_ERROR_NOT_INITIALIZED;
    if (userId == 0) return NP_PARTY_ERROR_INVALID_ARGUMENT;
    if (!ValidateInvitationListParam(param, true)) return NP_PARTY_ERROR_INVALID_ARGUMENT;
    return NP_PARTY_ERROR_NOT_IN_PARTY;
}

int APS5_VABI sceNpPartyTerminate(void) {
    std::lock_guard lock(g_handlerMutex);
    if (!g_initialized.exchange(false)) return NP_PARTY_ERROR_NOT_INITIALIZED;
    g_handlerCallback.store(nullptr);
    g_privateHandler.store(nullptr);
    g_handlerRegistered.store(false);
    return 0;
}

int APS5_VABI sceNpPartyUnregisterPrivateHandler(void) {
    std::lock_guard lock(g_handlerMutex);
    if (!g_initialized) return NP_PARTY_ERROR_NOT_INITIALIZED;
    if (!g_privateHandler.load()) return NP_PARTY_ERROR_HANDLER_STATE;
    g_privateHandler.store(nullptr);
    return 0;
}

}
