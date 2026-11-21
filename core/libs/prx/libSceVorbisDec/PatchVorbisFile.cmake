file(READ "${SOURCE}" vorbisfile)

function(replace_vorbisfile original replacement)
    string(FIND "${vorbisfile}" "${original}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "libvorbisfile changed at '${original}'. Review the 64-bit tell adapter.")
    endif()
    string(REPLACE "${original}" "${replacement}" result "${vorbisfile}")
    set(vorbisfile "${result}" PARENT_SCOPE)
endfunction()

replace_vorbisfile("static int _open_seekable2(OggVorbis_File *vf)"
    "static int _open_seekable2(OggVorbis_File *vf,ogg_int64_t (*tell64)(void *))")
replace_vorbisfile("vf->callbacks.seek_func && vf->callbacks.tell_func"
    "vf->callbacks.seek_func && (tell64 || vf->callbacks.tell_func)")
replace_vorbisfile("vf->offset=vf->end=(vf->callbacks.tell_func)(vf->datasource)"
    "vf->offset=vf->end=tell64 ? tell64(vf->datasource) : (vf->callbacks.tell_func)(vf->datasource)")
replace_vorbisfile("static int _ov_open2(OggVorbis_File *vf)"
    "int anyps5_ov_test_open64(OggVorbis_File *vf,ogg_int64_t (*tell64)(void *))")
replace_vorbisfile("int ret=_open_seekable2(vf);" "int ret=_open_seekable2(vf,tell64);")
replace_vorbisfile("return _ov_open2(vf);" "return anyps5_ov_test_open64(vf,NULL);")

file(CONFIGURE OUTPUT "${OUTPUT}" CONTENT "${vorbisfile}" @ONLY)
