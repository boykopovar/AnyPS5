# libSceVideodec2

## change [Unsupported HEVC metadata projections]

- [Unsupported HEVC metadata projections](../../../core/libs/prx/libSceVideodec2/HevcMetadata.cpp): crop/default-display windows, temporal sublayers, multilayer streams, SPS extensions, VPS-only timing, unavailable parameter sets, reserved VCL NAL types (10-15, 22-31), pictures missing `first_slice_segment_in_pic_flag` or mixing PPS IDs, and invalid SEI placement (prefix-only messages after a slice or in suffix NALs, type 132 outside suffix NALs, prefix SEI after the last slice, or suffix SEI before the first slice per H.265 7.4.2.4.4) throw at query time after the common validity-word clears. Differing parameter sets under an existing ID and malformed parameter histories keep queries unsupported through Reset until decoder recreation. Alternative-transfer persistence follows CLVS boundaries; conflicts, pictureless messages and non-IRAP continuation after reset/flush reject metadata. Decode behavior is retained.
