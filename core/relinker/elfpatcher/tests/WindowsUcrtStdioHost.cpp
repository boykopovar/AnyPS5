struct _iobuf;

extern "C" _iobuf* __cdecl __acrt_iob_func(unsigned index);
extern "C" int __cdecl fputs(const char*, _iobuf*);
extern "C" int __cdecl fflush(_iobuf*);

extern "C" int shared() {
    auto* output = __acrt_iob_func(1);
    auto* error = __acrt_iob_func(2);
    fputs("Windows DLL stdout probe\n", output);
    fputs("Windows DLL stderr probe\n", error);
    fflush(output);
    fflush(error);
    return 11;
}
