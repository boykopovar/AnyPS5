# libcairo

## add Functional

- [ScaledFont.cpp](../../../core/libs/prx/libcairo/ScaledFont.cpp) - No guest-callable export currently creates a native scaled-font handle: `cairo_scaled_font_create` still throws and contexts expose no scaled-font getter. These seven native-backed lifecycle and metric exports therefore require an already valid Cairo-owned handle. Non-null guest UTF-8 is checked copied with a 1 MiB limit. Invalid or unterminated strings, null handles on queries other than `cairo_scaled_font_status`, which answers `CAIRO_STATUS_NULL_POINTER` for a null handle without a console measurement, and unreadable output pointers fail through `NotImplementedAt_nid_no_patch`, since source behavior for guest address faults is unknown. Outputs use `GuestCopyOut_nid_no_patch` rather than passing guest pointers to Cairo. Native Cairo 1.18.0 supplies the reference counts and metrics; PS5 hardware behavior is unverified.

## remove `cairo_scaled_font_destroy`

## remove `cairo_scaled_font_extents`

## remove `cairo_scaled_font_get_font_matrix`

## remove `cairo_scaled_font_text_extents`

## remove `cairo_scaled_font_status`

## remove `cairo_scaled_font_get_ctm`

## remove `cairo_scaled_font_reference`
