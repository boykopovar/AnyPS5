#include <windows.h>

extern "C" {
EXCEPTION_DISPOSITION __gxx_personality_seh0(EXCEPTION_RECORD*, void*, CONTEXT*, void*);
EXCEPTION_DISPOSITION __gcc_personality_seh0(EXCEPTION_RECORD*, void*, CONTEXT*, void*);

[[gnu::section(".sehgcc"), gnu::used]] extern const PEXCEPTION_ROUTINE LibcSehPersonalities_nid_no_patch[] {
    __gxx_personality_seh0,
    __gcc_personality_seh0,
};
}
