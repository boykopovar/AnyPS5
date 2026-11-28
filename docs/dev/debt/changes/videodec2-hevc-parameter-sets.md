# libSceVideodec2

## change AVC parameter-set units
- [AVC and HEVC parameter-set units](../../../core/libs/prx/libSceVideodec2/Export.cpp) (libSceVideodec2) - an H.264 access unit with no slice NAL (types 1 to 5), such as the sequence and picture parameter sets PPSA99474 submits on their own before a stream's first slices, or an HEVC access unit with no VCL slice NAL (types 0 to 31), is parsed into the decoder's metadata state, kept and put in front of the next unit that has a slice, and `sceVideodec2Decode` returns 0 with no picture for it, since libavcodec refuses a packet without a slice; cumulative pending parameter sets are bounded to 2 MiB before insertion. What the console returns for such a unit is not known.
