#include "prx/libharfbuzz/include/Layout.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"

#include <hb-ot.h>

#include <algorithm>
#include <cstddef>
#include <type_traits>
#include <vector>

static_assert(std::is_same_v<hb_tag_t, std::uint32_t>);
static_assert(std::is_same_v<unsigned int, std::uint32_t>);
static_assert(std::is_same_v<hb_bool_t, int>);

namespace {

bool ReadGuest(void* destination, const void* source, std::size_t bytes) {
    const bool ok = GuestCopyIn_nid_no_patch(destination, source, bytes);
    return ok;
}

bool WritableGuest(void* destination, std::size_t bytes) {
    const bool ok = GuestWritable_nid_no_patch(destination, bytes);
    return ok;
}

bool WriteGuest(void* destination, const void* source, std::size_t bytes) {
    const bool ok = GuestCopyOut_nid_no_patch(destination, source, bytes);
    return ok;
}

template <typename TQuery>
std::uint32_t Enumerate(hb_face_t* face, std::uint32_t start, std::uint32_t* count, std::uint32_t* tags, const char* caller, TQuery query) {
    if (!face) NotImplemented_nid_no_patch(caller);
    if (!count) return query(nullptr, nullptr);
    std::uint32_t hostCount = 0;
    if (!ReadGuest(&hostCount, count, sizeof(hostCount))) NotImplemented_nid_no_patch(caller);
    const std::uint32_t total = query(nullptr, nullptr);
    const std::uint32_t written = std::min(hostCount, total - std::min(total, start));
    if (written && !tags) NotImplemented_nid_no_patch(caller);
    GuestArena::HostWrite countWrite(count, sizeof(*count));
    GuestArena::HostWrite tagsWrite(written ? tags : nullptr, static_cast<std::size_t>(written) * sizeof(*tags));
    if (!countWrite.Open() || !tagsWrite.Open()) NotImplemented_nid_no_patch(caller);
    if (!WritableGuest(count, sizeof(*count))) NotImplemented_nid_no_patch(caller);
    if (written && !WritableGuest(tags, static_cast<std::size_t>(written) * sizeof(*tags)))
        NotImplemented_nid_no_patch(caller);
    std::vector<std::uint32_t> hostTags(written);
    const std::uint32_t result = query(&hostCount, written ? hostTags.data() : nullptr);
    if (!WriteGuest(count, &hostCount, sizeof(hostCount))) NotImplemented_nid_no_patch(caller);
    if (written && !WriteGuest(tags, hostTags.data(), static_cast<std::size_t>(written) * sizeof(*tags)))
        NotImplemented_nid_no_patch(caller);
    return result;
}

}

extern "C" {

std::uint32_t APS5_VABI hb_ot_layout_table_get_script_tags_nid_postfix(hb_face_t* face, std::uint32_t table, std::uint32_t start, std::uint32_t* count, std::uint32_t* tags) {
    return Enumerate(face, start, count, tags, __func__, [=](std::uint32_t* outputCount, std::uint32_t* outputTags) {
        return hb_ot_layout_table_get_script_tags(face, table, start, outputCount, outputTags);
    });
}

std::uint32_t APS5_VABI hb_ot_layout_script_get_language_tags_nid_postfix(hb_face_t* face, std::uint32_t table, std::uint32_t script, std::uint32_t start, std::uint32_t* count, std::uint32_t* tags) {
    return Enumerate(face, start, count, tags, __func__, [=](std::uint32_t* outputCount, std::uint32_t* outputTags) {
        return hb_ot_layout_script_get_language_tags(face, table, script, start, outputCount, outputTags);
    });
}

int APS5_VABI hb_ot_layout_language_find_feature_nid_postfix(hb_face_t* face, std::uint32_t table, std::uint32_t script, std::uint32_t language, std::uint32_t feature, std::uint32_t* index) {
    if (!face) NotImplemented_nid_no_patch(__func__);
    GuestArena::HostWrite indexWrite(index, index ? sizeof(*index) : 0);
    if (!indexWrite.Open()) NotImplemented_nid_no_patch(__func__);
    if (index && !WritableGuest(index, sizeof(*index))) NotImplemented_nid_no_patch(__func__);
    std::uint32_t hostIndex = 0;
    const int found = hb_ot_layout_language_find_feature(face, table, script, language, feature, index ? &hostIndex : nullptr);
    if (index && !WriteGuest(index, &hostIndex, sizeof(hostIndex))) NotImplemented_nid_no_patch(__func__);
    return found;
}

}
