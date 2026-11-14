# libwebrtc

## change [197 exports (59 without a known name, bound by NID)]

- [195 exports (59 without a known name, bound by NID)](../../../core/libs/prx/libwebrtc/Export.cpp) (libwebrtc) - every function export of `libwebrtc.sprx` from the firmware 13.60 export table that the next section does not implement, names from aerolib.csv only where `tools/nid_names.py` `compute_nid` reproduces the NID. Signatures, parameter layouts and behaviour are unknown; every export throws through `NotImplemented_nid_no_patch`

## add Public source implementations

- [webrtc::rtcp::CompoundPacket constructor and destructor](../../../core/libs/prx/libwebrtc/src/RtcpCompoundPacket.cpp) (libwebrtc) - layout, vtable and allocator are unverified on the console. Layout: vtable, `sender_ssrc_` at 8, the `std::vector<std::unique_ptr<RtcpPacket>>` of appended packets at 16. The destructor calls each packet's deleting destructor (vtable slot 1) and frees the vector storage with the module allocator shared with `AudioEncoder::EncodedInfo`. A local vtable holds D1, D0, `BlockLength` and `Create`, which forward to the appended packets; `Append` and `Build` are not exported. Adapted from [compound_packet.cc](https://webrtc.googlesource.com/src/+/8d78f5de6c27b2c793039989ea381f1428fb0100/modules/rtp_rtcp/source/rtcp_packet/compound_packet.cc), the same at M127. Closes when: compared with the console.
