#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceAvcap2mvr/include/InventoryExports.hpp"
#include <cstddef>
#include <cstdint>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include <stdexcept>

namespace {

constexpr int ErrorInvalidArgument = static_cast<int>(0x81AC0001u);
constexpr std::int64_t NoStream = -1;
constexpr std::size_t StreamWordOffset = 0x70;

bool HasEngine() {
    return false;
}

template <typename T>
T ReadStream(const void* stream, std::size_t offset) {
    T value{};
    if (!GuestCopyIn_nid_no_patch(&value, static_cast<const std::byte*>(stream) + offset, sizeof(value))) {
        throw std::invalid_argument("libSceAvcap2mvr: unreadable stream");
    }
    return value;
}

bool IsOpenStream(const void* stream) {
    return ReadStream<std::uint8_t>(stream, 0) == 1;
}

std::int64_t StreamWord(const void* stream) {
    return ReadStream<std::int64_t>(stream, StreamWordOffset);
}

} // namespace

extern "C" {

APS5_DUMMY_FUN

}

extern "C" {

int APS5_VABI libSceAvcap2mvrUnknown_AVuH0FujF30_nid_no_patch(const void* argument1, const void* argument2) {
    if (argument1 == nullptr || argument2 == nullptr) return ErrorInvalidArgument;
    if (!HasEngine()) return ErrorInvalidArgument;
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("AVuH0FujF30", libSceAvcap2mvrUnknown_AVuH0FujF30_nid_no_patch);

int APS5_VABI libSceAvcap2mvrUnknown_JgAC1LkbWns_nid_no_patch(const void* stream) {
    if (stream == nullptr || !IsOpenStream(stream)) return ErrorInvalidArgument;
    if (!HasEngine()) return ErrorInvalidArgument;
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("JgAC1LkbWns", libSceAvcap2mvrUnknown_JgAC1LkbWns_nid_no_patch);

int APS5_VABI libSceAvcap2mvrUnknown_Kt2IOGEfaFc_nid_no_patch(const void* stream, const void* argument2) {
    if (stream == nullptr || argument2 == nullptr) return ErrorInvalidArgument;
    if (!IsOpenStream(stream) || StreamWord(stream) == NoStream) return ErrorInvalidArgument;
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("Kt2IOGEfaFc", libSceAvcap2mvrUnknown_Kt2IOGEfaFc_nid_no_patch);

int APS5_VABI libSceAvcap2mvrUnknown_Mq1MuGlr48g_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("Mq1MuGlr48g", libSceAvcap2mvrUnknown_Mq1MuGlr48g_nid_no_patch);

int APS5_VABI libSceAvcap2mvrUnknown_QSLdUawhEhg_nid_no_patch(const void* argument1, const void* argument2) {
    if (argument1 == nullptr || argument2 == nullptr) return ErrorInvalidArgument;
    if (!HasEngine()) return ErrorInvalidArgument;
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("QSLdUawhEhg", libSceAvcap2mvrUnknown_QSLdUawhEhg_nid_no_patch);

int APS5_VABI libSceAvcap2mvrUnknown_R5tFB0tMbgY_nid_no_patch(const void* stream, const void* argument2) {
    if (stream == nullptr || argument2 == nullptr) return ErrorInvalidArgument;
    if (!IsOpenStream(stream)) return ErrorInvalidArgument;
    if (!HasEngine()) return ErrorInvalidArgument;
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("R5tFB0tMbgY", libSceAvcap2mvrUnknown_R5tFB0tMbgY_nid_no_patch);

int APS5_VABI libSceAvcap2mvrUnknown_Tjmx_plus_DOF65c_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("Tjmx+DOF65c", libSceAvcap2mvrUnknown_Tjmx_plus_DOF65c_nid_no_patch);

int APS5_VABI libSceAvcap2mvrUnknown_ZMR_plus_Q0NYcmU_nid_no_patch(const void* stream) {
    if (stream == nullptr || !IsOpenStream(stream) || StreamWord(stream) == NoStream) return ErrorInvalidArgument;
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("ZMR+Q0NYcmU", libSceAvcap2mvrUnknown_ZMR_plus_Q0NYcmU_nid_no_patch);

int APS5_VABI libSceAvcap2mvrUnknown_nPlusHmt9NY_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("nPlusHmt9NY", libSceAvcap2mvrUnknown_nPlusHmt9NY_nid_no_patch);

int APS5_VABI libSceAvcap2mvrUnknown_s7aQGkLitBU_nid_no_patch(const void* stream, const void* argument2) {
    if (stream == nullptr || argument2 == nullptr) return ErrorInvalidArgument;
    if (!IsOpenStream(stream)) return ErrorInvalidArgument;
    if (!HasEngine()) return ErrorInvalidArgument;
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("s7aQGkLitBU", libSceAvcap2mvrUnknown_s7aQGkLitBU_nid_no_patch);

int APS5_VABI libSceAvcap2mvrUnknown_sM27f1nWpCc_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("sM27f1nWpCc", libSceAvcap2mvrUnknown_sM27f1nWpCc_nid_no_patch);

int APS5_VABI libSceAvcap2mvrUnknown_tq_minus_PG8QaNDM_nid_no_patch(const void* stream, const void* argument2) {
    if (stream == nullptr || argument2 == nullptr) return ErrorInvalidArgument;
    if (!IsOpenStream(stream)) return ErrorInvalidArgument;
    if (!HasEngine()) return ErrorInvalidArgument;
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("tq-PG8QaNDM", libSceAvcap2mvrUnknown_tq_minus_PG8QaNDM_nid_no_patch);

int APS5_VABI libSceAvcap2mvrUnknown_u4wtg4AegL8_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("u4wtg4AegL8", libSceAvcap2mvrUnknown_u4wtg4AegL8_nid_no_patch);

}
