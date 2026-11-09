struct _iobuf {
    char* ptr;
    int count;
    char* base;
    int flags;
    int file;
    int charbuf;
    int bufferSize;
    char* tempName;
};

static_assert(sizeof(_iobuf) == 48);

extern "C" _iobuf* __cdecl __iob_func();
extern "C" int __cdecl fputs(const char*, _iobuf*);
extern "C" int __cdecl fflush(_iobuf*);

extern "C" int shared() {
    auto* streams = __iob_func();
    fputs("Windows DLL stdout probe\n", streams + 1);
    fputs("Windows DLL stderr probe\n", streams + 2);
    fflush(streams + 1);
    fflush(streams + 2);
    return 11;
}
