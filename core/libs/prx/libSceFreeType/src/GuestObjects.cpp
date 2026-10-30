#include "prx/libSceFreeType/include/GuestObjects.hpp"

#include <cstddef>
#include <stdexcept>

namespace FreeType {

GuestFace* FaceOf(const GuestFaceRec* face) {
    return face ? static_cast<GuestFace*>(face->internal) : nullptr;
}

GuestFace* FaceOf(const GuestGlyphSlotRec* slot) {
    return slot ? static_cast<GuestFace*>(slot->internal) : nullptr;
}

void SyncFace(GuestFace& face) {
    RegisterBitmap_nid_no_patch(face);
    const FT_Face host = face.host;
    auto& rec = face.rec;
    rec.num_faces = host->num_faces;
    rec.face_index = host->face_index;
    rec.face_flags = host->face_flags;
    rec.style_flags = host->style_flags;
    rec.num_glyphs = host->num_glyphs;
    rec.family_name = host->family_name;
    rec.style_name = host->style_name;
    face.fixedSizes.resize(static_cast<std::size_t>(host->num_fixed_sizes));
    for (std::size_t index = 0; index < face.fixedSizes.size(); ++index) {
        const auto& size = host->available_sizes[index];
        face.fixedSizes[index] = {size.height, size.width, size.size, size.x_ppem, size.y_ppem};
    }
    rec.num_fixed_sizes = host->num_fixed_sizes;
    rec.available_sizes = face.fixedSizes.empty() ? nullptr : face.fixedSizes.data();
    face.charmaps.resize(static_cast<std::size_t>(host->num_charmaps));
    face.charmapPointers.resize(face.charmaps.size());
    rec.charmap = nullptr;
    for (std::size_t index = 0; index < face.charmaps.size(); ++index) {
        const FT_CharMap charmap = host->charmaps[index];
        face.charmaps[index] = {&rec, static_cast<std::int32_t>(charmap->encoding), charmap->platform_id, charmap->encoding_id};
        face.charmapPointers[index] = &face.charmaps[index];
        if (charmap == host->charmap) rec.charmap = &face.charmaps[index];
    }
    rec.num_charmaps = host->num_charmaps;
    rec.charmaps = face.charmapPointers.empty() ? nullptr : face.charmapPointers.data();
    rec.bbox = {host->bbox.xMin, host->bbox.yMin, host->bbox.xMax, host->bbox.yMax};
    rec.units_per_EM = host->units_per_EM;
    rec.ascender = host->ascender;
    rec.descender = host->descender;
    rec.height = host->height;
    rec.max_advance_width = host->max_advance_width;
    rec.max_advance_height = host->max_advance_height;
    rec.underline_position = host->underline_position;
    rec.underline_thickness = host->underline_thickness;
    rec.glyph = host->glyph ? &face.slot : nullptr;
    rec.internal = &face;
    SyncSize(face);
    SyncSlot(face);
}

void SyncSizeRecord_nid_no_patch(GuestFace& face, GuestSizeRec& rec, FT_Size host) {
    rec.face = &face.rec;
    rec.internal = host;
    if (!host) {
        rec.metrics = {};
        return;
    }
    const auto& metrics = host->metrics;
    rec.metrics = {metrics.x_ppem, metrics.y_ppem, metrics.x_scale, metrics.y_scale, metrics.ascender, metrics.descender, metrics.height, metrics.max_advance};
}

void SyncSizePointer_nid_no_patch(GuestFace& face) {
    const FT_Size active = face.host->size;
    if (!active) {
        face.rec.size = nullptr;
        return;
    }
    if (active == face.initialSize) {
        face.rec.size = &face.size;
        return;
    }
    for (auto& size : face.sizes) {
        if (size.host != active) continue;
        face.rec.size = &size.rec;
        return;
    }
    throw std::runtime_error("FreeType: an active size created outside the guest bridge is not implemented");
}

void SyncSize(GuestFace& face) {
    if (!face.sizeInitialized) {
        face.initialSize = face.host->size;
        face.sizeInitialized = true;
        SyncSizeRecord_nid_no_patch(face, face.size, face.initialSize);
    }
    SyncSizePointer_nid_no_patch(face);
    if (face.rec.size) SyncSizeRecord_nid_no_patch(face, *face.rec.size, face.host->size);
}

void SyncSlot(GuestFace& face) {
    auto& slot = face.slot;
    slot.library = face.library;
    slot.face = &face.rec;
    slot.next = nullptr;
    slot.internal = &face;
    const FT_GlyphSlot host = face.host->glyph;
    if (!host) return;
    slot.glyph_index = host->glyph_index;
    const auto& metrics = host->metrics;
    slot.metrics = {metrics.width, metrics.height, metrics.horiBearingX, metrics.horiBearingY, metrics.horiAdvance, metrics.vertBearingX, metrics.vertBearingY, metrics.vertAdvance};
    slot.linearHoriAdvance = host->linearHoriAdvance;
    slot.linearVertAdvance = host->linearVertAdvance;
    slot.advance = {host->advance.x, host->advance.y};
    slot.format = static_cast<std::uint32_t>(host->format);
    const auto& bitmap = host->bitmap;
    slot.bitmap = {bitmap.rows, bitmap.width, bitmap.pitch, bitmap.buffer, bitmap.num_grays, bitmap.pixel_mode, bitmap.palette_mode, bitmap.palette};
    slot.bitmap_left = host->bitmap_left;
    slot.bitmap_top = host->bitmap_top;
    const auto& outline = host->outline;
    face.outlinePoints.resize(outline.n_points);
    for (std::size_t index = 0; index < face.outlinePoints.size(); ++index) face.outlinePoints[index] = {outline.points[index].x, outline.points[index].y};
    slot.outline = {outline.n_contours, outline.n_points, face.outlinePoints.empty() ? nullptr : face.outlinePoints.data(), outline.tags, outline.contours, outline.flags};
    slot.num_subglyphs = host->num_subglyphs;
    slot.subglyphs = nullptr;
    slot.control_data = host->control_data;
    slot.control_len = host->control_len;
    slot.lsb_delta = host->lsb_delta;
    slot.rsb_delta = host->rsb_delta;
    slot.other = host->other;
}

}
