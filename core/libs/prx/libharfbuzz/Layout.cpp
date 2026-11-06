#include "prx/libharfbuzz/include/Layout.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"

#include <hb-ot.h>

#include <algorithm>
#include <cstddef>
#include <type_traits>

static_assert(std::is_same_v<hb_tag_t, std::uint32_t>);
static_assert(std::is_same_v<unsigned int, std::uint32_t>);
static_assert(std::is_same_v<hb_bool_t, int>);

namespace {

template <typename TQuery>
std::uint32_t Enumerate(hb_face_t* face, std::uint32_t start, std::uint32_t* count, std::uint32_t* tags, TQuery query) {
    if (!face) NotImplemented_nid_no_patch(__func__);
    if (!count) return query(nullptr, nullptr);
    const std::uint32_t total = query(nullptr, nullptr);
    const std::uint32_t written = std::min(*count, total - std::min(total, start));
    if (written && !tags) NotImplemented_nid_no_patch(__func__);
    GuestArena::HostWrite countWrite(count, sizeof(*count));
    GuestArena::HostWrite tagsWrite(written ? tags : nullptr, static_cast<std::size_t>(written) * sizeof(*tags));
    if (!countWrite.Open() || !tagsWrite.Open()) NotImplemented_nid_no_patch(__func__);
    return query(count, tags);
}

}

extern "C" {

std::uint32_t APS5_VABI hb_ot_layout_table_get_script_tags_nid_postfix(hb_face_t* face, std::uint32_t table, std::uint32_t start, std::uint32_t* count, std::uint32_t* tags) {
    return Enumerate(face, start, count, tags, [=](std::uint32_t* outputCount, std::uint32_t* outputTags) {
        return hb_ot_layout_table_get_script_tags(face, table, start, outputCount, outputTags);
    });
}

std::uint32_t APS5_VABI hb_ot_layout_script_get_language_tags_nid_postfix(hb_face_t* face, std::uint32_t table, std::uint32_t script, std::uint32_t start, std::uint32_t* count, std::uint32_t* tags) {
    return Enumerate(face, start, count, tags, [=](std::uint32_t* outputCount, std::uint32_t* outputTags) {
        return hb_ot_layout_script_get_language_tags(face, table, script, start, outputCount, outputTags);
    });
}

int APS5_VABI hb_ot_layout_language_find_feature_nid_postfix(hb_face_t* face, std::uint32_t table, std::uint32_t script, std::uint32_t language, std::uint32_t feature, std::uint32_t* index) {
    if (!face) NotImplemented_nid_no_patch(__func__);
    GuestArena::HostWrite indexWrite(index, index ? sizeof(*index) : 0);
    if (!indexWrite.Open()) NotImplemented_nid_no_patch(__func__);
    return hb_ot_layout_language_find_feature(face, table, script, language, feature, index);
}

}
