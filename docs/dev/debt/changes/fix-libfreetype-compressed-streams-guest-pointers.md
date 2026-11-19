# libfreetype

## add Functional

- [FT_Stream_OpenGzip and FT_Stream_OpenLZW](../../../core/libs/prx/libfreetype/src/CompressedStreams.cpp) (libfreetype) - stream and source pointers are validated with GuestWritable_nid_no_patch and accessed through GuestCopyIn_nid_no_patch and GuestCopyOut_nid_no_patch with caller errno preserved; returning FT_Err_Invalid_Stream_Handle on unwritable pointers is host safety fallback with console behavior unmeasured.
