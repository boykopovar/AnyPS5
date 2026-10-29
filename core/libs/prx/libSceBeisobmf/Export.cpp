#include "SceTypes.hpp"
#include "prx/libSceBeisobmf/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <cstddef>
#include <cstdint>
#include <array>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <unordered_set>

namespace {

std::uintptr_t FieldAddress(const void *object, std::size_t offset) {
    const auto address = reinterpret_cast<std::uintptr_t>(object);
    if (address == 0) throw std::invalid_argument("isobmf object pointer is null");
    if (offset > std::numeric_limits<std::uintptr_t>::max() - address)
        throw std::runtime_error("isobmf field address overflows");
    return address + offset;
}

template <typename TValue>
TValue ReadAt(std::uintptr_t address) {
    TValue value{};
    if (address == 0 ||
        !GuestCopyIn_nid_no_patch(&value, reinterpret_cast<const void *>(address), sizeof(value)))
        throw std::runtime_error("isobmf guest field is unreadable");
    return value;
}

template <typename TValue>
TValue ReadField(const void *object, std::size_t offset) {
    return ReadAt<TValue>(FieldAddress(object, offset));
}

std::uintptr_t ReadPointer(const void *object, std::size_t offset) {
    return ReadField<std::uintptr_t>(object, offset);
}

void WritePointer(void *object, std::size_t offset, std::uintptr_t value) {
    const auto address = FieldAddress(object, offset);
    if (!GuestCopyOut_nid_no_patch(reinterpret_cast<void *>(address), &value, sizeof(value)))
        throw std::runtime_error("isobmf guest cursor is not writable");
}

std::array<char, 0x40> CopyRequestedName(const char *name) {
    std::array<char, 0x40> result{};
    const auto address = reinterpret_cast<std::uintptr_t>(name);
    if (address == 0) throw std::invalid_argument("isobmf member name is null");
    for (std::size_t i = 0; i < result.size(); ++i) {
        if (i > std::numeric_limits<std::uintptr_t>::max() - address ||
            !GuestCopyIn_nid_no_patch(&result[i], reinterpret_cast<const void *>(address + i),
                                      sizeof(result[i])))
            throw std::runtime_error("isobmf member name is unreadable");
        if (result[i] == '\0') break;
    }
    return result;
}

std::uintptr_t FindMember(void *box, const char *name) {
    if (!box || !name)
        throw std::invalid_argument("isobmf member lookup requires a box and name");
    const auto requestedName = CopyRequestedName(name);
    const auto boxAddress = reinterpret_cast<std::uintptr_t>(box);
    WritePointer(box, 0x50, ReadPointer(box, 0x40));
    std::unordered_set<std::uintptr_t> visited;
    auto member = ReadPointer(box, 0x50);
    while (member != 0) {
        if (!visited.insert(member).second)
            throw std::runtime_error("isobmf member list contains a cycle");
        std::array<char, 0x40> memberName{};
        const auto nameAddress = FieldAddress(reinterpret_cast<const void *>(member), 0x18);
        if (!GuestCopyIn_nid_no_patch(memberName.data(),
                                      reinterpret_cast<const void *>(nameAddress), memberName.size()))
            throw std::runtime_error("isobmf member key is unreadable");
        if (std::strncmp(memberName.data(), requestedName.data(), memberName.size()) == 0)
            return member;
        member = ReadPointer(reinterpret_cast<const void *>(member), 0x10);
        WritePointer(reinterpret_cast<void *>(boxAddress), 0x50, member);
    }
    return 0;
}

void *MemberPointer(std::uintptr_t member) {
    const auto type =
        ReadField<std::uint32_t>(reinterpret_cast<const void *>(member), 0x6c);
    if (type < 20 && ((0x97800u >> (type & 0x1f)) & 1u) != 0)
        return reinterpret_cast<void *>(
            ReadPointer(reinterpret_cast<const void *>(member), 0x60));
    return reinterpret_cast<void *>(FieldAddress(reinterpret_cast<const void *>(member), 0x58));
}

void LogGuestAccessFailure(const char *function, const char *message) noexcept {
    try {
        APS5_LOG_ERR("%s failed safely: %s", function, message);
    } catch (...) {
    }
}

template <typename TValue, typename TFunction>
TValue CallSafely(const char *function, TValue fallback, TFunction operation) noexcept {
    try {
        return operation();
    } catch (const std::exception &error) {
        LogGuestAccessFailure(function, error.what());
        return fallback;
    } catch (...) {
        LogGuestAccessFailure(function, "unknown guest memory error");
        return fallback;
    }
}

} // namespace

extern "C" {

void *APS5_VABI isobmf_box_getchildbox_nid_postfix(void *box) {
    return CallSafely(__func__, static_cast<void *>(nullptr), [&] {
        return reinterpret_cast<void *>(ReadPointer(box, 0x10));
    });
}

std::uint64_t APS5_VABI isobmf_box_getdiskoffset_nid_postfix(void *box) {
    return CallSafely(__func__, std::uint64_t{0}, [&] { return ReadField<std::uint64_t>(box, 0x80); });
}

std::uint32_t APS5_VABI isobmf_box_getflags32b_nid_postfix(void *box) {
    return CallSafely(__func__, std::uint32_t{0}, [&] {
        if (!box) return std::uint32_t{0};
        const auto member = FindMember(box, "flags");
        if (!member) return std::uint32_t{0};
        std::array<std::uint8_t, 3> value{};
        const auto address = reinterpret_cast<std::uintptr_t>(MemberPointer(member));
        if (!GuestCopyIn_nid_no_patch(value.data(), reinterpret_cast<const void *>(address),
                                      value.size()))
            throw std::runtime_error("isobmf flags payload is unreadable");
        return static_cast<std::uint32_t>(value[0]) |
               (static_cast<std::uint32_t>(value[1]) << 8) |
               (static_cast<std::uint32_t>(value[2]) << 16);
    });
}

std::uint32_t APS5_VABI
isobmf_box_getmember_length_nid_postfix(void *box, const char *name) {
    return CallSafely(__func__, std::uint32_t{0}, [&] {
        const auto member = FindMember(box, name);
        return member ? ReadField<std::uint32_t>(reinterpret_cast<const void *>(member), 0x68) : 0;
    });
}

void *APS5_VABI isobmf_box_getmember_ptr_nid_postfix(void *box,
                                                     const char *name) {
    return CallSafely(__func__, static_cast<void *>(nullptr), [&] {
        const auto member = FindMember(box, name);
        return member ? MemberPointer(member) : nullptr;
    });
}

std::uint32_t APS5_VABI
isobmf_box_getmember_type_nid_postfix(void *box, const char *name) {
    return CallSafely(__func__, std::uint32_t{0}, [&] {
        const auto member = FindMember(box, name);
        return member ? ReadField<std::uint32_t>(reinterpret_cast<const void *>(member), 0x6c) : 0;
    });
}

void *APS5_VABI isobmf_box_getnextbox_nid_postfix(void *box) {
    return CallSafely(__func__, static_cast<void *>(nullptr), [&] {
        return reinterpret_cast<void *>(ReadPointer(box, 0x20));
    });
}

std::uint32_t APS5_VABI isobmf_box_getstate_nid_postfix(void *box) {
    return CallSafely(__func__, std::uint32_t{0}, [&] { return ReadField<std::uint32_t>(box, 0x8c); });
}

std::uint32_t APS5_VABI isobmf_box_gettype_nid_postfix(void *box) {
    return CallSafely(__func__, std::uint32_t{0}, [&] { return ReadField<std::uint32_t>(box, 0x90); });
}

int APS5_VABI isobmf_decoder_clear_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI isobmf_decoder_create_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI isobmf_decoder_decode_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI isobmf_decoder_destroy_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI isobmf_decoder_get_decoder_tree_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI isobmf_decoder_getquerydata_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

std::uint64_t APS5_VABI
isobmf_decoder_next_disk_offset_nid_postfix(void *decoder) {
    return CallSafely(__func__, std::uint64_t{0xffffffff}, [&] {
        if (!decoder) return std::uint64_t{0xffffffff};
        return ReadField<std::uint64_t>(decoder, 0xc0);
    });
}

int APS5_VABI isobmf_decoder_query_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI isobmf_memmanager_set_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
