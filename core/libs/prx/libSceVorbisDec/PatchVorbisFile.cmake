file(READ "${SOURCE}" vorbisfile)

function(replace_vorbisfile original replacement)
    string(FIND "${vorbisfile}" "${original}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "libvorbisfile changed at '${original}'. Review the guest 64-bit adapters.")
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

replace_vorbisfile("long ov_bitrate(OggVorbis_File *vf,int i)" "ogg_int64_t anyps5_ov_bitrate64(OggVorbis_File *vf,int i)")
replace_vorbisfile("ov_bitrate(vf,0)" "anyps5_ov_bitrate64(vf,0)")
replace_vorbisfile("(vf->vi[i].bitrate_upper+vf->vi[i].bitrate_lower)/2"
    "((ogg_int64_t)vf->vi[i].bitrate_upper+vf->vi[i].bitrate_lower)/2")
replace_vorbisfile("long ov_bitrate_instant(OggVorbis_File *vf)" "ogg_int64_t anyps5_ov_bitrate_instant64(OggVorbis_File *vf)")
replace_vorbisfile("  long ret;\n  if(vf->ready_state<OPENED)" "  ogg_int64_t ret;\n  if(vf->ready_state<OPENED)")

string(APPEND vorbisfile [=[
long ov_bitrate(OggVorbis_File *vf,int i){
  return (long)anyps5_ov_bitrate64(vf,i);
}

long ov_bitrate_instant(OggVorbis_File *vf){
  return (long)anyps5_ov_bitrate_instant64(vf);
}
]=])

file(CONFIGURE OUTPUT "${OUTPUT}" CONTENT "${vorbisfile}" @ONLY)
