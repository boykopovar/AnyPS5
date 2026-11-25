#include "prx/libc/include/general/AsmMacros.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <stdexcept>
#include <string>

extern "C" [[noreturn]] void APS5_VABI NKWebKitNullDelegate_nid_no_patch(const char* function) {
    throw std::invalid_argument(std::string(function) + " requires an object with a delegate");
}

__asm__(".text\n" APS5_ASM_FUNCTION("_ZN7WebCore11MediaPlayer10cancelLoadEv")
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov 0x50(%rdi), %rdi\n"
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov (%rdi), %rax\n"
        "jmp *0x20(%rax)\n"
        "1:\n"
        "lea 2f(%rip), %rdi\n"
        "jmp " APS5_ASM_SYMBOL("NKWebKitNullDelegate_nid_no_patch") "\n"
        "2:\n"
        ".asciz \"_ZN7WebCore11MediaPlayer10cancelLoadEv\"\n"
        APS5_ASM_FUNCTION_END("_ZN7WebCore11MediaPlayer10cancelLoadEv"));

__asm__(".text\n" APS5_ASM_FUNCTION("_ZN7WebCore11MediaPlayer10readyStateEv")
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov 0x50(%rdi), %rdi\n"
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov (%rdi), %rax\n"
        "jmp *0x1b0(%rax)\n"
        "1:\n"
        "lea 2f(%rip), %rdi\n"
        "jmp " APS5_ASM_SYMBOL("NKWebKitNullDelegate_nid_no_patch") "\n"
        "2:\n"
        ".asciz \"_ZN7WebCore11MediaPlayer10readyStateEv\"\n"
        APS5_ASM_FUNCTION_END("_ZN7WebCore11MediaPlayer10readyStateEv"));

__asm__(".text\n" APS5_ASM_FUNCTION("_ZN7WebCore11MediaPlayer11naturalSizeEv")
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov 0x50(%rdi), %rdi\n"
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov (%rdi), %rax\n"
        "jmp *0x90(%rax)\n"
        "1:\n"
        "lea 2f(%rip), %rdi\n"
        "jmp " APS5_ASM_SYMBOL("NKWebKitNullDelegate_nid_no_patch") "\n"
        "2:\n"
        ".asciz \"_ZN7WebCore11MediaPlayer11naturalSizeEv\"\n"
        APS5_ASM_FUNCTION_END("_ZN7WebCore11MediaPlayer11naturalSizeEv"));

__asm__(".text\n" APS5_ASM_FUNCTION("_ZN7WebCore11MediaPlayer11rateChangedEv")
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov 0x18(%rdi), %rdi\n"
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov (%rdi), %rax\n"
        "jmp *0x40(%rax)\n"
        "1:\n"
        "lea 2f(%rip), %rdi\n"
        "jmp " APS5_ASM_SYMBOL("NKWebKitNullDelegate_nid_no_patch") "\n"
        "2:\n"
        ".asciz \"_ZN7WebCore11MediaPlayer11rateChangedEv\"\n"
        APS5_ASM_FUNCTION_END("_ZN7WebCore11MediaPlayer11rateChangedEv"));

__asm__(".text\n" APS5_ASM_FUNCTION("_ZN7WebCore11MediaPlayer11sizeChangedEv")
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov 0x18(%rdi), %rdi\n"
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov (%rdi), %rax\n"
        "jmp *0x60(%rax)\n"
        "1:\n"
        "lea 2f(%rip), %rdi\n"
        "jmp " APS5_ASM_SYMBOL("NKWebKitNullDelegate_nid_no_patch") "\n"
        "2:\n"
        ".asciz \"_ZN7WebCore11MediaPlayer11sizeChangedEv\"\n"
        APS5_ASM_FUNCTION_END("_ZN7WebCore11MediaPlayer11sizeChangedEv"));

__asm__(".text\n" APS5_ASM_FUNCTION("_ZN7WebCore11MediaPlayer11timeChangedEv")
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov 0x18(%rdi), %rdi\n"
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov (%rdi), %rax\n"
        "jmp *0x30(%rax)\n"
        "1:\n"
        "lea 2f(%rip), %rdi\n"
        "jmp " APS5_ASM_SYMBOL("NKWebKitNullDelegate_nid_no_patch") "\n"
        "2:\n"
        ".asciz \"_ZN7WebCore11MediaPlayer11timeChangedEv\"\n"
        APS5_ASM_FUNCTION_END("_ZN7WebCore11MediaPlayer11timeChangedEv"));

__asm__(".text\n" APS5_ASM_FUNCTION("_ZN7WebCore11MediaPlayer12addTextTrackERNS_22InbandTextTrackPrivateE")
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov 0x18(%rdi), %rdi\n"
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov (%rdi), %rax\n"
        "jmp *0x128(%rax)\n"
        "1:\n"
        "lea 2f(%rip), %rdi\n"
        "jmp " APS5_ASM_SYMBOL("NKWebKitNullDelegate_nid_no_patch") "\n"
        "2:\n"
        ".asciz \"_ZN7WebCore11MediaPlayer12addTextTrackERNS_22InbandTextTrackPrivateE\"\n"
        APS5_ASM_FUNCTION_END("_ZN7WebCore11MediaPlayer12addTextTrackERNS_22InbandTextTrackPrivateE"));

__asm__(".text\n" APS5_ASM_FUNCTION("_ZN7WebCore11MediaPlayer12networkStateEv")
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov 0x50(%rdi), %rdi\n"
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov (%rdi), %rax\n"
        "jmp *0x1a8(%rax)\n"
        "1:\n"
        "lea 2f(%rip), %rdi\n"
        "jmp " APS5_ASM_SYMBOL("NKWebKitNullDelegate_nid_no_patch") "\n"
        "2:\n"
        ".asciz \"_ZN7WebCore11MediaPlayer12networkStateEv\"\n"
        APS5_ASM_FUNCTION_END("_ZN7WebCore11MediaPlayer12networkStateEv"));

__asm__(".text\n" APS5_ASM_FUNCTION("_ZN7WebCore11MediaPlayer13addAudioTrackERNS_17AudioTrackPrivateE")
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov 0x18(%rdi), %rdi\n"
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov (%rdi), %rax\n"
        "jmp *0x120(%rax)\n"
        "1:\n"
        "lea 2f(%rip), %rdi\n"
        "jmp " APS5_ASM_SYMBOL("NKWebKitNullDelegate_nid_no_patch") "\n"
        "2:\n"
        ".asciz \"_ZN7WebCore11MediaPlayer13addAudioTrackERNS_17AudioTrackPrivateE\"\n"
        APS5_ASM_FUNCTION_END("_ZN7WebCore11MediaPlayer13addAudioTrackERNS_17AudioTrackPrivateE"));

__asm__(".text\n" APS5_ASM_FUNCTION("_ZN7WebCore11MediaPlayer13addVideoTrackERNS_17VideoTrackPrivateE")
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov 0x18(%rdi), %rdi\n"
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov (%rdi), %rax\n"
        "jmp *0x130(%rax)\n"
        "1:\n"
        "lea 2f(%rip), %rdi\n"
        "jmp " APS5_ASM_SYMBOL("NKWebKitNullDelegate_nid_no_patch") "\n"
        "2:\n"
        ".asciz \"_ZN7WebCore11MediaPlayer13addVideoTrackERNS_17VideoTrackPrivateE\"\n"
        APS5_ASM_FUNCTION_END("_ZN7WebCore11MediaPlayer13addVideoTrackERNS_17VideoTrackPrivateE"));

__asm__(".text\n" APS5_ASM_FUNCTION("_ZN7WebCore11MediaPlayer13tracksChangedEv")
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov 0x50(%rdi), %rdi\n"
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov (%rdi), %rax\n"
        "jmp *0x2f0(%rax)\n"
        "1:\n"
        "lea 2f(%rip), %rdi\n"
        "jmp " APS5_ASM_SYMBOL("NKWebKitNullDelegate_nid_no_patch") "\n"
        "2:\n"
        ".asciz \"_ZN7WebCore11MediaPlayer13tracksChangedEv\"\n"
        APS5_ASM_FUNCTION_END("_ZN7WebCore11MediaPlayer13tracksChangedEv"));

__asm__(".text\n" APS5_ASM_FUNCTION("_ZN7WebCore11MediaPlayer15durationChangedEv")
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov 0x18(%rdi), %rdi\n"
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov (%rdi), %rax\n"
        "jmp *0x38(%rax)\n"
        "1:\n"
        "lea 2f(%rip), %rdi\n"
        "jmp " APS5_ASM_SYMBOL("NKWebKitNullDelegate_nid_no_patch") "\n"
        "2:\n"
        ".asciz \"_ZN7WebCore11MediaPlayer15durationChangedEv\"\n"
        APS5_ASM_FUNCTION_END("_ZN7WebCore11MediaPlayer15durationChangedEv"));

__asm__(".text\n" APS5_ASM_FUNCTION("_ZN7WebCore11MediaPlayer15removeTextTrackERNS_22InbandTextTrackPrivateE")
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov 0x18(%rdi), %rdi\n"
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov (%rdi), %rax\n"
        "jmp *0x140(%rax)\n"
        "1:\n"
        "lea 2f(%rip), %rdi\n"
        "jmp " APS5_ASM_SYMBOL("NKWebKitNullDelegate_nid_no_patch") "\n"
        "2:\n"
        ".asciz \"_ZN7WebCore11MediaPlayer15removeTextTrackERNS_22InbandTextTrackPrivateE\"\n"
        APS5_ASM_FUNCTION_END("_ZN7WebCore11MediaPlayer15removeTextTrackERNS_22InbandTextTrackPrivateE"));

__asm__(".text\n" APS5_ASM_FUNCTION("_ZN7WebCore11MediaPlayer16removeAudioTrackERNS_17AudioTrackPrivateE")
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov 0x18(%rdi), %rdi\n"
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov (%rdi), %rax\n"
        "jmp *0x138(%rax)\n"
        "1:\n"
        "lea 2f(%rip), %rdi\n"
        "jmp " APS5_ASM_SYMBOL("NKWebKitNullDelegate_nid_no_patch") "\n"
        "2:\n"
        ".asciz \"_ZN7WebCore11MediaPlayer16removeAudioTrackERNS_17AudioTrackPrivateE\"\n"
        APS5_ASM_FUNCTION_END("_ZN7WebCore11MediaPlayer16removeAudioTrackERNS_17AudioTrackPrivateE"));

__asm__(".text\n" APS5_ASM_FUNCTION("_ZN7WebCore11MediaPlayer16removeVideoTrackERNS_17VideoTrackPrivateE")
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov 0x18(%rdi), %rdi\n"
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov (%rdi), %rax\n"
        "jmp *0x148(%rax)\n"
        "1:\n"
        "lea 2f(%rip), %rdi\n"
        "jmp " APS5_ASM_SYMBOL("NKWebKitNullDelegate_nid_no_patch") "\n"
        "2:\n"
        ".asciz \"_ZN7WebCore11MediaPlayer16removeVideoTrackERNS_17VideoTrackPrivateE\"\n"
        APS5_ASM_FUNCTION_END("_ZN7WebCore11MediaPlayer16removeVideoTrackERNS_17VideoTrackPrivateE"));

__asm__(".text\n" APS5_ASM_FUNCTION("_ZN7WebCore11MediaPlayer17seekWithToleranceERKN3WTF9MediaTimeES4_S4_")
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov 0x50(%rdi), %rdi\n"
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov (%rdi), %rax\n"
        "jmp *0x118(%rax)\n"
        "1:\n"
        "lea 2f(%rip), %rdi\n"
        "jmp " APS5_ASM_SYMBOL("NKWebKitNullDelegate_nid_no_patch") "\n"
        "2:\n"
        ".asciz \"_ZN7WebCore11MediaPlayer17seekWithToleranceERKN3WTF9MediaTimeES4_S4_\"\n"
        APS5_ASM_FUNCTION_END("_ZN7WebCore11MediaPlayer17seekWithToleranceERKN3WTF9MediaTimeES4_S4_"));

__asm__(".text\n" APS5_ASM_FUNCTION("_ZN7WebCore11MediaPlayer18liveUpdateIntervalEv")
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov 0x50(%rdi), %rdi\n"
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov (%rdi), %rax\n"
        "jmp *0x1f0(%rax)\n"
        "1:\n"
        "lea 2f(%rip), %rdi\n"
        "jmp " APS5_ASM_SYMBOL("NKWebKitNullDelegate_nid_no_patch") "\n"
        "2:\n"
        ".asciz \"_ZN7WebCore11MediaPlayer18liveUpdateIntervalEv\"\n"
        APS5_ASM_FUNCTION_END("_ZN7WebCore11MediaPlayer18liveUpdateIntervalEv"));

__asm__(".text\n" APS5_ASM_FUNCTION("_ZN7WebCore11MediaPlayer18mediaEngineUpdatedEv")
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov 0x18(%rdi), %rdi\n"
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov (%rdi), %rax\n"
        "jmp *0x68(%rax)\n"
        "1:\n"
        "lea 2f(%rip), %rdi\n"
        "jmp " APS5_ASM_SYMBOL("NKWebKitNullDelegate_nid_no_patch") "\n"
        "2:\n"
        ".asciz \"_ZN7WebCore11MediaPlayer18mediaEngineUpdatedEv\"\n"
        APS5_ASM_FUNCTION_END("_ZN7WebCore11MediaPlayer18mediaEngineUpdatedEv"));

__asm__(".text\n" APS5_ASM_FUNCTION("_ZN7WebCore11MediaPlayer18setBufferingPolicyENS_16MediaPlayerEnums15BufferingPolicyE")
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov 0x50(%rdi), %rdi\n"
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov (%rdi), %rax\n"
        "jmp *0x58(%rax)\n"
        "1:\n"
        "lea 2f(%rip), %rdi\n"
        "jmp " APS5_ASM_SYMBOL("NKWebKitNullDelegate_nid_no_patch") "\n"
        "2:\n"
        ".asciz \"_ZN7WebCore11MediaPlayer18setBufferingPolicyENS_16MediaPlayerEnums15BufferingPolicyE\"\n"
        APS5_ASM_FUNCTION_END("_ZN7WebCore11MediaPlayer18setBufferingPolicyENS_16MediaPlayerEnums15BufferingPolicyE"));

__asm__(".text\n" APS5_ASM_FUNCTION("_ZN7WebCore11MediaPlayer19syncTextTrackBoundsEv")
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov 0x50(%rdi), %rdi\n"
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov (%rdi), %rax\n"
        "jmp *0x2e8(%rax)\n"
        "1:\n"
        "lea 2f(%rip), %rdi\n"
        "jmp " APS5_ASM_SYMBOL("NKWebKitNullDelegate_nid_no_patch") "\n"
        "2:\n"
        ".asciz \"_ZN7WebCore11MediaPlayer19syncTextTrackBoundsEv\"\n"
        APS5_ASM_FUNCTION_END("_ZN7WebCore11MediaPlayer19syncTextTrackBoundsEv"));

__asm__(".text\n" APS5_ASM_FUNCTION("_ZN7WebCore11MediaPlayer20cachedResourceLoaderEv")
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov 0x18(%rdi), %rdi\n"
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov (%rdi), %rax\n"
        "jmp *0xf8(%rax)\n"
        "1:\n"
        "lea 2f(%rip), %rdi\n"
        "jmp " APS5_ASM_SYMBOL("NKWebKitNullDelegate_nid_no_patch") "\n"
        "2:\n"
        ".asciz \"_ZN7WebCore11MediaPlayer20cachedResourceLoaderEv\"\n"
        APS5_ASM_FUNCTION_END("_ZN7WebCore11MediaPlayer20cachedResourceLoaderEv"));

__asm__(".text\n" APS5_ASM_FUNCTION("_ZN7WebCore11MediaPlayer20playbackStateChangedEv")
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov 0x18(%rdi), %rdi\n"
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov (%rdi), %rax\n"
        "jmp *0x48(%rax)\n"
        "1:\n"
        "lea 2f(%rip), %rdi\n"
        "jmp " APS5_ASM_SYMBOL("NKWebKitNullDelegate_nid_no_patch") "\n"
        "2:\n"
        ".asciz \"_ZN7WebCore11MediaPlayer20playbackStateChangedEv\"\n"
        APS5_ASM_FUNCTION_END("_ZN7WebCore11MediaPlayer20playbackStateChangedEv"));

__asm__(".text\n" APS5_ASM_FUNCTION("_ZN7WebCore11MediaPlayer20resourceNotSupportedEv")
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov 0x18(%rdi), %rdi\n"
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov (%rdi), %rax\n"
        "jmp *0x50(%rax)\n"
        "1:\n"
        "lea 2f(%rip), %rdi\n"
        "jmp " APS5_ASM_SYMBOL("NKWebKitNullDelegate_nid_no_patch") "\n"
        "2:\n"
        ".asciz \"_ZN7WebCore11MediaPlayer20resourceNotSupportedEv\"\n"
        APS5_ASM_FUNCTION_END("_ZN7WebCore11MediaPlayer20resourceNotSupportedEv"));

__asm__(".text\n" APS5_ASM_FUNCTION("_ZN7WebCore11MediaPlayer21characteristicChangedEv")
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov 0x18(%rdi), %rdi\n"
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov (%rdi), %rax\n"
        "jmp *0x78(%rax)\n"
        "1:\n"
        "lea 2f(%rip), %rdi\n"
        "jmp " APS5_ASM_SYMBOL("NKWebKitNullDelegate_nid_no_patch") "\n"
        "2:\n"
        ".asciz \"_ZN7WebCore11MediaPlayer21characteristicChangedEv\"\n"
        APS5_ASM_FUNCTION_END("_ZN7WebCore11MediaPlayer21characteristicChangedEv"));

__asm__(".text\n" APS5_ASM_FUNCTION("_ZN7WebCore11MediaPlayer22performTaskAtMediaTimeEON3WTF8FunctionIFvvEEERKNS1_9MediaTimeE")
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov 0x50(%rdi), %rdi\n"
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov (%rdi), %rax\n"
        "jmp *0x358(%rax)\n"
        "1:\n"
        "lea 2f(%rip), %rdi\n"
        "jmp " APS5_ASM_SYMBOL("NKWebKitNullDelegate_nid_no_patch") "\n"
        "2:\n"
        ".asciz \"_ZN7WebCore11MediaPlayer22performTaskAtMediaTimeEON3WTF8FunctionIFvvEEERKNS1_9MediaTimeE\"\n"
        APS5_ASM_FUNCTION_END("_ZN7WebCore11MediaPlayer22performTaskAtMediaTimeEON3WTF8FunctionIFvvEEERKNS1_9MediaTimeE"));

__asm__(".text\n" APS5_ASM_FUNCTION("_ZN7WebCore11MediaPlayer24firstVideoFrameAvailableEv")
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov 0x18(%rdi), %rdi\n"
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov (%rdi), %rax\n"
        "jmp *0x70(%rax)\n"
        "1:\n"
        "lea 2f(%rip), %rdi\n"
        "jmp " APS5_ASM_SYMBOL("NKWebKitNullDelegate_nid_no_patch") "\n"
        "2:\n"
        ".asciz \"_ZN7WebCore11MediaPlayer24firstVideoFrameAvailableEv\"\n"
        APS5_ASM_FUNCTION_END("_ZN7WebCore11MediaPlayer24firstVideoFrameAvailableEv"));

__asm__(".text\n" APS5_ASM_FUNCTION("_ZN7WebCore11MediaPlayer24remoteEngineFailedToLoadEv")
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov 0x18(%rdi), %rdi\n"
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov (%rdi), %rax\n"
        "jmp *0x178(%rax)\n"
        "1:\n"
        "lea 2f(%rip), %rdi\n"
        "jmp " APS5_ASM_SYMBOL("NKWebKitNullDelegate_nid_no_patch") "\n"
        "2:\n"
        ".asciz \"_ZN7WebCore11MediaPlayer24remoteEngineFailedToLoadEv\"\n"
        APS5_ASM_FUNCTION_END("_ZN7WebCore11MediaPlayer24remoteEngineFailedToLoadEv"));

__asm__(".text\n" APS5_ASM_FUNCTION("_ZN7WebCore11MediaPlayer24setClosedCaptionsVisibleEb")
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov 0x50(%rdi), %rdi\n"
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov (%rdi), %rax\n"
        "jmp *0x190(%rax)\n"
        "1:\n"
        "lea 2f(%rip), %rdi\n"
        "jmp " APS5_ASM_SYMBOL("NKWebKitNullDelegate_nid_no_patch") "\n"
        "2:\n"
        ".asciz \"_ZN7WebCore11MediaPlayer24setClosedCaptionsVisibleEb\"\n"
        APS5_ASM_FUNCTION_END("_ZN7WebCore11MediaPlayer24setClosedCaptionsVisibleEb"));

__asm__(".text\n" APS5_ASM_FUNCTION("_ZN7WebCore11MediaPlayer25bufferedTimeRangesChangedEv")
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov 0x18(%rdi), %rdi\n"
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov (%rdi), %rax\n"
        "jmp *0x1b8(%rax)\n"
        "1:\n"
        "lea 2f(%rip), %rdi\n"
        "jmp " APS5_ASM_SYMBOL("NKWebKitNullDelegate_nid_no_patch") "\n"
        "2:\n"
        ".asciz \"_ZN7WebCore11MediaPlayer25bufferedTimeRangesChangedEv\"\n"
        APS5_ASM_FUNCTION_END("_ZN7WebCore11MediaPlayer25bufferedTimeRangesChangedEv"));

__asm__(".text\n" APS5_ASM_FUNCTION("_ZN7WebCore11MediaPlayer25seekableTimeRangesChangedEv")
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov 0x18(%rdi), %rdi\n"
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov (%rdi), %rax\n"
        "jmp *0x1c0(%rax)\n"
        "1:\n"
        "lea 2f(%rip), %rdi\n"
        "jmp " APS5_ASM_SYMBOL("NKWebKitNullDelegate_nid_no_patch") "\n"
        "2:\n"
        ".asciz \"_ZN7WebCore11MediaPlayer25seekableTimeRangesChangedEv\"\n"
        APS5_ASM_FUNCTION_END("_ZN7WebCore11MediaPlayer25seekableTimeRangesChangedEv"));

__asm__(".text\n" APS5_ASM_FUNCTION("_ZN7WebCore11MediaPlayer25shouldIgnoreIntrinsicSizeEv")
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov 0x50(%rdi), %rdi\n"
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov (%rdi), %rax\n"
        "jmp *0x360(%rax)\n"
        "1:\n"
        "lea 2f(%rip), %rdi\n"
        "jmp " APS5_ASM_SYMBOL("NKWebKitNullDelegate_nid_no_patch") "\n"
        "2:\n"
        ".asciz \"_ZN7WebCore11MediaPlayer25shouldIgnoreIntrinsicSizeEv\"\n"
        APS5_ASM_FUNCTION_END("_ZN7WebCore11MediaPlayer25shouldIgnoreIntrinsicSizeEv"));

__asm__(".text\n" APS5_ASM_FUNCTION("_ZN7WebCore11MediaPlayer26activeSourceBuffersChangedEv")
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov 0x18(%rdi), %rdi\n"
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov (%rdi), %rax\n"
        "jmp *0x98(%rax)\n"
        "1:\n"
        "lea 2f(%rip), %rdi\n"
        "jmp " APS5_ASM_SYMBOL("NKWebKitNullDelegate_nid_no_patch") "\n"
        "2:\n"
        ".asciz \"_ZN7WebCore11MediaPlayer26activeSourceBuffersChangedEv\"\n"
        APS5_ASM_FUNCTION_END("_ZN7WebCore11MediaPlayer26activeSourceBuffersChangedEv"));

__asm__(".text\n" APS5_ASM_FUNCTION("_ZN7WebCore11MediaPlayer26applicationDidBecomeActiveEv")
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov 0x50(%rdi), %rdi\n"
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov (%rdi), %rax\n"
        "jmp *0x350(%rax)\n"
        "1:\n"
        "lea 2f(%rip), %rdi\n"
        "jmp " APS5_ASM_SYMBOL("NKWebKitNullDelegate_nid_no_patch") "\n"
        "2:\n"
        ".asciz \"_ZN7WebCore11MediaPlayer26applicationDidBecomeActiveEv\"\n"
        APS5_ASM_FUNCTION_END("_ZN7WebCore11MediaPlayer26applicationDidBecomeActiveEv"));

__asm__(".text\n" APS5_ASM_FUNCTION("_ZN7WebCore11MediaPlayer26setTextTrackRepresentationEPNS_23TextTrackRepresentationE")
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov 0x50(%rdi), %rdi\n"
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov (%rdi), %rax\n"
        "jmp *0x2e0(%rax)\n"
        "1:\n"
        "lea 2f(%rip), %rdi\n"
        "jmp " APS5_ASM_SYMBOL("NKWebKitNullDelegate_nid_no_patch") "\n"
        "2:\n"
        ".asciz \"_ZN7WebCore11MediaPlayer26setTextTrackRepresentationEPNS_23TextTrackRepresentationE\"\n"
        APS5_ASM_FUNCTION_END("_ZN7WebCore11MediaPlayer26setTextTrackRepresentationEPNS_23TextTrackRepresentationE"));

__asm__(".text\n" APS5_ASM_FUNCTION("_ZN7WebCore11MediaPlayer27applicationWillResignActiveEv")
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov 0x50(%rdi), %rdi\n"
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov (%rdi), %rax\n"
        "jmp *0x348(%rax)\n"
        "1:\n"
        "lea 2f(%rip), %rdi\n"
        "jmp " APS5_ASM_SYMBOL("NKWebKitNullDelegate_nid_no_patch") "\n"
        "2:\n"
        ".asciz \"_ZN7WebCore11MediaPlayer27applicationWillResignActiveEv\"\n"
        APS5_ASM_FUNCTION_END("_ZN7WebCore11MediaPlayer27applicationWillResignActiveEv"));

__asm__(".text\n" APS5_ASM_FUNCTION("_ZN7WebCore11MediaPlayer28setShouldMaintainAspectRatioEb")
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov 0x50(%rdi), %rdi\n"
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov (%rdi), %rax\n"
        "jmp *0x268(%rax)\n"
        "1:\n"
        "lea 2f(%rip), %rdi\n"
        "jmp " APS5_ASM_SYMBOL("NKWebKitNullDelegate_nid_no_patch") "\n"
        "2:\n"
        ".asciz \"_ZN7WebCore11MediaPlayer28setShouldMaintainAspectRatioEb\"\n"
        APS5_ASM_FUNCTION_END("_ZN7WebCore11MediaPlayer28setShouldMaintainAspectRatioEb"));

__asm__(".text\n" APS5_ASM_FUNCTION("_ZN7WebCore11MediaPlayer29acceleratedCompositingEnabledEv")
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov 0x18(%rdi), %rdi\n"
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov (%rdi), %rax\n"
        "jmp *0x90(%rax)\n"
        "1:\n"
        "lea 2f(%rip), %rdi\n"
        "jmp " APS5_ASM_SYMBOL("NKWebKitNullDelegate_nid_no_patch") "\n"
        "2:\n"
        ".asciz \"_ZN7WebCore11MediaPlayer29acceleratedCompositingEnabledEv\"\n"
        APS5_ASM_FUNCTION_END("_ZN7WebCore11MediaPlayer29acceleratedCompositingEnabledEv"));

__asm__(".text\n" APS5_ASM_FUNCTION("_ZN7WebCore11MediaPlayer32acceleratedRenderingStateChangedEv")
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov 0x50(%rdi), %rdi\n"
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov (%rdi), %rax\n"
        "jmp *0x260(%rax)\n"
        "1:\n"
        "lea 2f(%rip), %rdi\n"
        "jmp " APS5_ASM_SYMBOL("NKWebKitNullDelegate_nid_no_patch") "\n"
        "2:\n"
        ".asciz \"_ZN7WebCore11MediaPlayer32acceleratedRenderingStateChangedEv\"\n"
        APS5_ASM_FUNCTION_END("_ZN7WebCore11MediaPlayer32acceleratedRenderingStateChangedEv"));

__asm__(".text\n" APS5_ASM_FUNCTION("_ZN7WebCore11MediaPlayer34seekableTimeRangesLastModifiedTimeEv")
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov 0x50(%rdi), %rdi\n"
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov (%rdi), %rax\n"
        "jmp *0x1e8(%rax)\n"
        "1:\n"
        "lea 2f(%rip), %rdi\n"
        "jmp " APS5_ASM_SYMBOL("NKWebKitNullDelegate_nid_no_patch") "\n"
        "2:\n"
        ".asciz \"_ZN7WebCore11MediaPlayer34seekableTimeRangesLastModifiedTimeEv\"\n"
        APS5_ASM_FUNCTION_END("_ZN7WebCore11MediaPlayer34seekableTimeRangesLastModifiedTimeEv"));

__asm__(".text\n" APS5_ASM_FUNCTION("_ZN7WebCore11MediaPlayer4playEv")
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov 0x50(%rdi), %rdi\n"
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov (%rdi), %rax\n"
        "jmp *0x48(%rax)\n"
        "1:\n"
        "lea 2f(%rip), %rdi\n"
        "jmp " APS5_ASM_SYMBOL("NKWebKitNullDelegate_nid_no_patch") "\n"
        "2:\n"
        ".asciz \"_ZN7WebCore11MediaPlayer4playEv\"\n"
        APS5_ASM_FUNCTION_END("_ZN7WebCore11MediaPlayer4playEv"));

__asm__(".text\n" APS5_ASM_FUNCTION("_ZN7WebCore11MediaPlayer4seekERKN3WTF9MediaTimeE")
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov 0x50(%rdi), %rdi\n"
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov (%rdi), %rax\n"
        "jmp *0x110(%rax)\n"
        "1:\n"
        "lea 2f(%rip), %rdi\n"
        "jmp " APS5_ASM_SYMBOL("NKWebKitNullDelegate_nid_no_patch") "\n"
        "2:\n"
        ".asciz \"_ZN7WebCore11MediaPlayer4seekERKN3WTF9MediaTimeE\"\n"
        APS5_ASM_FUNCTION_END("_ZN7WebCore11MediaPlayer4seekERKN3WTF9MediaTimeE"));

__asm__(".text\n" APS5_ASM_FUNCTION("_ZN7WebCore11MediaPlayer5paintERNS_15GraphicsContextERKNS_9FloatRectE")
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov 0x50(%rdi), %rdi\n"
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov (%rdi), %rax\n"
        "jmp *0x218(%rax)\n"
        "1:\n"
        "lea 2f(%rip), %rdi\n"
        "jmp " APS5_ASM_SYMBOL("NKWebKitNullDelegate_nid_no_patch") "\n"
        "2:\n"
        ".asciz \"_ZN7WebCore11MediaPlayer5paintERNS_15GraphicsContextERKNS_9FloatRectE\"\n"
        APS5_ASM_FUNCTION_END("_ZN7WebCore11MediaPlayer5paintERNS_15GraphicsContextERKNS_9FloatRectE"));

__asm__(".text\n" APS5_ASM_FUNCTION("_ZN7WebCore11MediaPlayer5pauseEv")
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov 0x50(%rdi), %rdi\n"
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov (%rdi), %rax\n"
        "jmp *0x50(%rax)\n"
        "1:\n"
        "lea 2f(%rip), %rdi\n"
        "jmp " APS5_ASM_SYMBOL("NKWebKitNullDelegate_nid_no_patch") "\n"
        "2:\n"
        ".asciz \"_ZN7WebCore11MediaPlayer5pauseEv\"\n"
        APS5_ASM_FUNCTION_END("_ZN7WebCore11MediaPlayer5pauseEv"));

__asm__(".text\n" APS5_ASM_FUNCTION("_ZN7WebCore11MediaPlayer7repaintEv")
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov 0x18(%rdi), %rdi\n"
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov (%rdi), %rax\n"
        "jmp *0x58(%rax)\n"
        "1:\n"
        "lea 2f(%rip), %rdi\n"
        "jmp " APS5_ASM_SYMBOL("NKWebKitNullDelegate_nid_no_patch") "\n"
        "2:\n"
        ".asciz \"_ZN7WebCore11MediaPlayer7repaintEv\"\n"
        APS5_ASM_FUNCTION_END("_ZN7WebCore11MediaPlayer7repaintEv"));

__asm__(".text\n" APS5_ASM_FUNCTION("_ZN7WebCore11MediaPlayer7setRateEd")
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov 0x50(%rdi), %rdi\n"
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov (%rdi), %rax\n"
        "jmp *0x140(%rax)\n"
        "1:\n"
        "lea 2f(%rip), %rdi\n"
        "jmp " APS5_ASM_SYMBOL("NKWebKitNullDelegate_nid_no_patch") "\n"
        "2:\n"
        ".asciz \"_ZN7WebCore11MediaPlayer7setRateEd\"\n"
        APS5_ASM_FUNCTION_END("_ZN7WebCore11MediaPlayer7setRateEd"));

__asm__(".text\n" APS5_ASM_FUNCTION("_ZN7WebCore16HTMLInputElement11selectColorEN3WTF10StringViewE")
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov 0xf8(%rdi), %rdi\n"
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov (%rdi), %rax\n"
        "jmp *0x348(%rax)\n"
        "1:\n"
        "lea 2f(%rip), %rdi\n"
        "jmp " APS5_ASM_SYMBOL("NKWebKitNullDelegate_nid_no_patch") "\n"
        "2:\n"
        ".asciz \"_ZN7WebCore16HTMLInputElement11selectColorEN3WTF10StringViewE\"\n"
        APS5_ASM_FUNCTION_END("_ZN7WebCore16HTMLInputElement11selectColorEN3WTF10StringViewE"));

__asm__(".text\n" APS5_ASM_FUNCTION("_ZN7WebCore16HTMLInputElement5filesEv")
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov 0xf8(%rdi), %rdi\n"
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov (%rdi), %rax\n"
        "jmp *0x278(%rax)\n"
        "1:\n"
        "lea 2f(%rip), %rdi\n"
        "jmp " APS5_ASM_SYMBOL("NKWebKitNullDelegate_nid_no_patch") "\n"
        "2:\n"
        ".asciz \"_ZN7WebCore16HTMLInputElement5filesEv\"\n"
        APS5_ASM_FUNCTION_END("_ZN7WebCore16HTMLInputElement5filesEv"));

__asm__(".text\n" APS5_ASM_FUNCTION("_ZN7WebCore18RenderLayerBacking30setIsTrackingDisplayListReplayEb")
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov 0x38(%rdi), %rdi\n"
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov (%rdi), %rax\n"
        "jmp *0x2b0(%rax)\n"
        "1:\n"
        "lea 2f(%rip), %rdi\n"
        "jmp " APS5_ASM_SYMBOL("NKWebKitNullDelegate_nid_no_patch") "\n"
        "2:\n"
        ".asciz \"_ZN7WebCore18RenderLayerBacking30setIsTrackingDisplayListReplayEb\"\n"
        APS5_ASM_FUNCTION_END("_ZN7WebCore18RenderLayerBacking30setIsTrackingDisplayListReplayEb"));

__asm__(".text\n" APS5_ASM_FUNCTION("_ZN7WebCore22CanvasRenderingContext5derefEv")
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov 0x18(%rdi), %rdi\n"
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov (%rdi), %rax\n"
        "jmp *0x18(%rax)\n"
        "1:\n"
        "lea 2f(%rip), %rdi\n"
        "jmp " APS5_ASM_SYMBOL("NKWebKitNullDelegate_nid_no_patch") "\n"
        "2:\n"
        ".asciz \"_ZN7WebCore22CanvasRenderingContext5derefEv\"\n"
        APS5_ASM_FUNCTION_END("_ZN7WebCore22CanvasRenderingContext5derefEv"));

__asm__(".text\n" APS5_ASM_FUNCTION("_ZN7WebCore4Page23clearUndoRedoOperationsEv")
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov 0x78(%rdi), %rdi\n"
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov (%rdi), %rax\n"
        "jmp *0x128(%rax)\n"
        "1:\n"
        "lea 2f(%rip), %rdi\n"
        "jmp " APS5_ASM_SYMBOL("NKWebKitNullDelegate_nid_no_patch") "\n"
        "2:\n"
        ".asciz \"_ZN7WebCore4Page23clearUndoRedoOperationsEv\"\n"
        APS5_ASM_FUNCTION_END("_ZN7WebCore4Page23clearUndoRedoOperationsEv"));

__asm__(".text\n" APS5_ASM_FUNCTION("_ZN7WebCore6Chrome23enableSuddenTerminationEv")
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov 0x10(%rdi), %rdi\n"
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov (%rdi), %rax\n"
        "jmp *0x388(%rax)\n"
        "1:\n"
        "lea 2f(%rip), %rdi\n"
        "jmp " APS5_ASM_SYMBOL("NKWebKitNullDelegate_nid_no_patch") "\n"
        "2:\n"
        ".asciz \"_ZN7WebCore6Chrome23enableSuddenTerminationEv\"\n"
        APS5_ASM_FUNCTION_END("_ZN7WebCore6Chrome23enableSuddenTerminationEv"));

__asm__(".text\n" APS5_ASM_FUNCTION("_ZN7WebCore6Chrome24disableSuddenTerminationEv")
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov 0x10(%rdi), %rdi\n"
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov (%rdi), %rax\n"
        "jmp *0x390(%rax)\n"
        "1:\n"
        "lea 2f(%rip), %rdi\n"
        "jmp " APS5_ASM_SYMBOL("NKWebKitNullDelegate_nid_no_patch") "\n"
        "2:\n"
        ".asciz \"_ZN7WebCore6Chrome24disableSuddenTerminationEv\"\n"
        APS5_ASM_FUNCTION_END("_ZN7WebCore6Chrome24disableSuddenTerminationEv"));

__asm__(".text\n" APS5_ASM_FUNCTION("_ZNK7WebCore11FrameLoader6pageIDEv")
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov 0x8(%rdi), %rdi\n"
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov (%rdi), %rax\n"
        "jmp *0x28(%rax)\n"
        "1:\n"
        "lea 2f(%rip), %rdi\n"
        "jmp " APS5_ASM_SYMBOL("NKWebKitNullDelegate_nid_no_patch") "\n"
        "2:\n"
        ".asciz \"_ZNK7WebCore11FrameLoader6pageIDEv\"\n"
        APS5_ASM_FUNCTION_END("_ZNK7WebCore11FrameLoader6pageIDEv"));

__asm__(".text\n" APS5_ASM_FUNCTION("_ZNK7WebCore11FrameLoader7frameIDEv")
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov 0x8(%rdi), %rdi\n"
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov (%rdi), %rax\n"
        "jmp *0x30(%rax)\n"
        "1:\n"
        "lea 2f(%rip), %rdi\n"
        "jmp " APS5_ASM_SYMBOL("NKWebKitNullDelegate_nid_no_patch") "\n"
        "2:\n"
        ".asciz \"_ZNK7WebCore11FrameLoader7frameIDEv\"\n"
        APS5_ASM_FUNCTION_END("_ZNK7WebCore11FrameLoader7frameIDEv"));

__asm__(".text\n" APS5_ASM_FUNCTION("_ZNK7WebCore11MediaPlayer13isVideoPlayerEv")
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov 0x18(%rdi), %rdi\n"
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov (%rdi), %rax\n"
        "jmp *0xd0(%rax)\n"
        "1:\n"
        "lea 2f(%rip), %rdi\n"
        "jmp " APS5_ASM_SYMBOL("NKWebKitNullDelegate_nid_no_patch") "\n"
        "2:\n"
        ".asciz \"_ZNK7WebCore11MediaPlayer13isVideoPlayerEv\"\n"
        APS5_ASM_FUNCTION_END("_ZNK7WebCore11MediaPlayer13isVideoPlayerEv"));

__asm__(".text\n" APS5_ASM_FUNCTION("_ZNK7WebCore11MediaPlayer13movieLoadTypeEv")
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov 0x50(%rdi), %rdi\n"
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov (%rdi), %rax\n"
        "jmp *0x288(%rax)\n"
        "1:\n"
        "lea 2f(%rip), %rdi\n"
        "jmp " APS5_ASM_SYMBOL("NKWebKitNullDelegate_nid_no_patch") "\n"
        "2:\n"
        ".asciz \"_ZNK7WebCore11MediaPlayer13movieLoadTypeEv\"\n"
        APS5_ASM_FUNCTION_END("_ZNK7WebCore11MediaPlayer13movieLoadTypeEv"));

__asm__(".text\n" APS5_ASM_FUNCTION("_ZNK7WebCore11MediaPlayer13platformLayerEv")
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov 0x50(%rdi), %rdi\n"
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov (%rdi), %rax\n"
        "jmp *0x38(%rax)\n"
        "1:\n"
        "lea 2f(%rip), %rdi\n"
        "jmp " APS5_ASM_SYMBOL("NKWebKitNullDelegate_nid_no_patch") "\n"
        "2:\n"
        ".asciz \"_ZNK7WebCore11MediaPlayer13platformLayerEv\"\n"
        APS5_ASM_FUNCTION_END("_ZNK7WebCore11MediaPlayer13platformLayerEv"));

__asm__(".text\n" APS5_ASM_FUNCTION("_ZNK7WebCore11MediaPlayer13requestedRateEv")
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov 0x18(%rdi), %rdi\n"
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov (%rdi), %rax\n"
        "jmp *0x180(%rax)\n"
        "1:\n"
        "lea 2f(%rip), %rdi\n"
        "jmp " APS5_ASM_SYMBOL("NKWebKitNullDelegate_nid_no_patch") "\n"
        "2:\n"
        ".asciz \"_ZNK7WebCore11MediaPlayer13requestedRateEv\"\n"
        APS5_ASM_FUNCTION_END("_ZNK7WebCore11MediaPlayer13requestedRateEv"));

__asm__(".text\n" APS5_ASM_FUNCTION("_ZNK7WebCore11MediaPlayer16canSaveMediaDataEv")
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov 0x50(%rdi), %rdi\n"
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov (%rdi), %rax\n"
        "jmp *0x88(%rax)\n"
        "1:\n"
        "lea 2f(%rip), %rdi\n"
        "jmp " APS5_ASM_SYMBOL("NKWebKitNullDelegate_nid_no_patch") "\n"
        "2:\n"
        ".asciz \"_ZNK7WebCore11MediaPlayer16canSaveMediaDataEv\"\n"
        APS5_ASM_FUNCTION_END("_ZNK7WebCore11MediaPlayer16canSaveMediaDataEv"));

__asm__(".text\n" APS5_ASM_FUNCTION("_ZNK7WebCore11MediaPlayer16supportsScanningEv")
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov 0x50(%rdi), %rdi\n"
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov (%rdi), %rax\n"
        "jmp *0x70(%rax)\n"
        "1:\n"
        "lea 2f(%rip), %rdi\n"
        "jmp " APS5_ASM_SYMBOL("NKWebKitNullDelegate_nid_no_patch") "\n"
        "2:\n"
        ".asciz \"_ZNK7WebCore11MediaPlayer16supportsScanningEv\"\n"
        APS5_ASM_FUNCTION_END("_ZNK7WebCore11MediaPlayer16supportsScanningEv"));

__asm__(".text\n" APS5_ASM_FUNCTION("_ZNK7WebCore11MediaPlayer17decodedFrameCountEv")
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov 0x50(%rdi), %rdi\n"
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov (%rdi), %rax\n"
        "jmp *0x2a8(%rax)\n"
        "1:\n"
        "lea 2f(%rip), %rdi\n"
        "jmp " APS5_ASM_SYMBOL("NKWebKitNullDelegate_nid_no_patch") "\n"
        "2:\n"
        ".asciz \"_ZNK7WebCore11MediaPlayer17decodedFrameCountEv\"\n"
        APS5_ASM_FUNCTION_END("_ZNK7WebCore11MediaPlayer17decodedFrameCountEv"));

__asm__(".text\n" APS5_ASM_FUNCTION("_ZNK7WebCore11MediaPlayer17doesHaveAttributeERKN3WTF10AtomStringEPS2_")
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov 0x18(%rdi), %rdi\n"
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov (%rdi), %rax\n"
        "jmp *0x108(%rax)\n"
        "1:\n"
        "lea 2f(%rip), %rdi\n"
        "jmp " APS5_ASM_SYMBOL("NKWebKitNullDelegate_nid_no_patch") "\n"
        "2:\n"
        ".asciz \"_ZNK7WebCore11MediaPlayer17doesHaveAttributeERKN3WTF10AtomStringEPS2_\"\n"
        APS5_ASM_FUNCTION_END("_ZNK7WebCore11MediaPlayer17doesHaveAttributeERKN3WTF10AtomStringEPS2_"));

__asm__(".text\n" APS5_ASM_FUNCTION("_ZNK7WebCore11MediaPlayer17droppedFrameCountEv")
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov 0x50(%rdi), %rdi\n"
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov (%rdi), %rax\n"
        "jmp *0x2b0(%rax)\n"
        "1:\n"
        "lea 2f(%rip), %rdi\n"
        "jmp " APS5_ASM_SYMBOL("NKWebKitNullDelegate_nid_no_patch") "\n"
        "2:\n"
        ".asciz \"_ZNK7WebCore11MediaPlayer17droppedFrameCountEv\"\n"
        APS5_ASM_FUNCTION_END("_ZNK7WebCore11MediaPlayer17droppedFrameCountEv"));

__asm__(".text\n" APS5_ASM_FUNCTION("_ZNK7WebCore11MediaPlayer17hasClosedCaptionsEv")
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov 0x50(%rdi), %rdi\n"
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov (%rdi), %rax\n"
        "jmp *0x188(%rax)\n"
        "1:\n"
        "lea 2f(%rip), %rdi\n"
        "jmp " APS5_ASM_SYMBOL("NKWebKitNullDelegate_nid_no_patch") "\n"
        "2:\n"
        ".asciz \"_ZNK7WebCore11MediaPlayer17hasClosedCaptionsEv\"\n"
        APS5_ASM_FUNCTION_END("_ZNK7WebCore11MediaPlayer17hasClosedCaptionsEv"));

__asm__(".text\n" APS5_ASM_FUNCTION("_ZNK7WebCore11MediaPlayer18maxFastForwardRateEv")
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov 0x50(%rdi), %rdi\n"
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov (%rdi), %rax\n"
        "jmp *0x198(%rax)\n"
        "1:\n"
        "lea 2f(%rip), %rdi\n"
        "jmp " APS5_ASM_SYMBOL("NKWebKitNullDelegate_nid_no_patch") "\n"
        "2:\n"
        ".asciz \"_ZNK7WebCore11MediaPlayer18maxFastForwardRateEv\"\n"
        APS5_ASM_FUNCTION_END("_ZNK7WebCore11MediaPlayer18maxFastForwardRateEv"));

__asm__(".text\n" APS5_ASM_FUNCTION("_ZNK7WebCore11MediaPlayer18minFastReverseRateEv")
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov 0x50(%rdi), %rdi\n"
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov (%rdi), %rax\n"
        "jmp *0x1a0(%rax)\n"
        "1:\n"
        "lea 2f(%rip), %rdi\n"
        "jmp " APS5_ASM_SYMBOL("NKWebKitNullDelegate_nid_no_patch") "\n"
        "2:\n"
        ".asciz \"_ZNK7WebCore11MediaPlayer18minFastReverseRateEv\"\n"
        APS5_ASM_FUNCTION_END("_ZNK7WebCore11MediaPlayer18minFastReverseRateEv"));

__asm__(".text\n" APS5_ASM_FUNCTION("_ZNK7WebCore11MediaPlayer18shouldDisableSleepEv")
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov 0x18(%rdi), %rdi\n"
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov (%rdi), %rax\n"
        "jmp *0x1a0(%rax)\n"
        "1:\n"
        "lea 2f(%rip), %rdi\n"
        "jmp " APS5_ASM_SYMBOL("NKWebKitNullDelegate_nid_no_patch") "\n"
        "2:\n"
        ".asciz \"_ZNK7WebCore11MediaPlayer18shouldDisableSleepEv\"\n"
        APS5_ASM_FUNCTION_END("_ZNK7WebCore11MediaPlayer18shouldDisableSleepEv"));

__asm__(".text\n" APS5_ASM_FUNCTION("_ZNK7WebCore11MediaPlayer18supportsFullscreenEv")
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov 0x50(%rdi), %rdi\n"
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov (%rdi), %rax\n"
        "jmp *0x68(%rax)\n"
        "1:\n"
        "lea 2f(%rip), %rdi\n"
        "jmp " APS5_ASM_SYMBOL("NKWebKitNullDelegate_nid_no_patch") "\n"
        "2:\n"
        ".asciz \"_ZNK7WebCore11MediaPlayer18supportsFullscreenEv\"\n"
        APS5_ASM_FUNCTION_END("_ZNK7WebCore11MediaPlayer18supportsFullscreenEv"));

__asm__(".text\n" APS5_ASM_FUNCTION("_ZNK7WebCore11MediaPlayer19mediaCacheDirectoryEv")
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov 0x18(%rdi), %rdi\n"
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov (%rdi), %rax\n"
        "jmp *0x118(%rax)\n"
        "1:\n"
        "lea 2f(%rip), %rdi\n"
        "jmp " APS5_ASM_SYMBOL("NKWebKitNullDelegate_nid_no_patch") "\n"
        "2:\n"
        ".asciz \"_ZNK7WebCore11MediaPlayer19mediaCacheDirectoryEv\"\n"
        APS5_ASM_FUNCTION_END("_ZNK7WebCore11MediaPlayer19mediaCacheDirectoryEv"));

__asm__(".text\n" APS5_ASM_FUNCTION("_ZNK7WebCore11MediaPlayer19playerContentsScaleEv")
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov 0x18(%rdi), %rdi\n"
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov (%rdi), %rax\n"
        "jmp *0xe0(%rax)\n"
        "1:\n"
        "lea 2f(%rip), %rdi\n"
        "jmp " APS5_ASM_SYMBOL("NKWebKitNullDelegate_nid_no_patch") "\n"
        "2:\n"
        ".asciz \"_ZNK7WebCore11MediaPlayer19playerContentsScaleEv\"\n"
        APS5_ASM_FUNCTION_END("_ZNK7WebCore11MediaPlayer19playerContentsScaleEv"));

__asm__(".text\n" APS5_ASM_FUNCTION("_ZNK7WebCore11MediaPlayer20playerContentBoxRectEv")
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov 0x18(%rdi), %rdi\n"
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov (%rdi), %rax\n"
        "jmp *0xd8(%rax)\n"
        "1:\n"
        "lea 2f(%rip), %rdi\n"
        "jmp " APS5_ASM_SYMBOL("NKWebKitNullDelegate_nid_no_patch") "\n"
        "2:\n"
        ".asciz \"_ZNK7WebCore11MediaPlayer20playerContentBoxRectEv\"\n"
        APS5_ASM_FUNCTION_END("_ZNK7WebCore11MediaPlayer20playerContentBoxRectEv"));

__asm__(".text\n" APS5_ASM_FUNCTION("_ZNK7WebCore11MediaPlayer20renderingModeChangedEv")
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov 0x18(%rdi), %rdi\n"
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov (%rdi), %rax\n"
        "jmp *0x88(%rax)\n"
        "1:\n"
        "lea 2f(%rip), %rdi\n"
        "jmp " APS5_ASM_SYMBOL("NKWebKitNullDelegate_nid_no_patch") "\n"
        "2:\n"
        ".asciz \"_ZNK7WebCore11MediaPlayer20renderingModeChangedEv\"\n"
        APS5_ASM_FUNCTION_END("_ZNK7WebCore11MediaPlayer20renderingModeChangedEv"));

__asm__(".text\n" APS5_ASM_FUNCTION("_ZNK7WebCore11MediaPlayer21audioDecodedByteCountEv")
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov 0x50(%rdi), %rdi\n"
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov (%rdi), %rax\n"
        "jmp *0x2b8(%rax)\n"
        "1:\n"
        "lea 2f(%rip), %rdi\n"
        "jmp " APS5_ASM_SYMBOL("NKWebKitNullDelegate_nid_no_patch") "\n"
        "2:\n"
        ".asciz \"_ZNK7WebCore11MediaPlayer21audioDecodedByteCountEv\"\n"
        APS5_ASM_FUNCTION_END("_ZNK7WebCore11MediaPlayer21audioDecodedByteCountEv"));

__asm__(".text\n" APS5_ASM_FUNCTION("_ZNK7WebCore11MediaPlayer21videoDecodedByteCountEv")
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov 0x50(%rdi), %rdi\n"
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov (%rdi), %rax\n"
        "jmp *0x2c0(%rax)\n"
        "1:\n"
        "lea 2f(%rip), %rdi\n"
        "jmp " APS5_ASM_SYMBOL("NKWebKitNullDelegate_nid_no_patch") "\n"
        "2:\n"
        ".asciz \"_ZNK7WebCore11MediaPlayer21videoDecodedByteCountEv\"\n"
        APS5_ASM_FUNCTION_END("_ZNK7WebCore11MediaPlayer21videoDecodedByteCountEv"));

__asm__(".text\n" APS5_ASM_FUNCTION("_ZNK7WebCore11MediaPlayer22didPassCORSAccessCheckEv")
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov 0x50(%rdi), %rdi\n"
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov (%rdi), %rax\n"
        "jmp *0x278(%rax)\n"
        "1:\n"
        "lea 2f(%rip), %rdi\n"
        "jmp " APS5_ASM_SYMBOL("NKWebKitNullDelegate_nid_no_patch") "\n"
        "2:\n"
        ".asciz \"_ZNK7WebCore11MediaPlayer22didPassCORSAccessCheckEv\"\n"
        APS5_ASM_FUNCTION_END("_ZNK7WebCore11MediaPlayer22didPassCORSAccessCheckEv"));

__asm__(".text\n" APS5_ASM_FUNCTION("_ZNK7WebCore11MediaPlayer22hasAvailableVideoFrameEv")
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov 0x50(%rdi), %rdi\n"
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov (%rdi), %rax\n"
        "jmp *0x250(%rax)\n"
        "1:\n"
        "lea 2f(%rip), %rdi\n"
        "jmp " APS5_ASM_SYMBOL("NKWebKitNullDelegate_nid_no_patch") "\n"
        "2:\n"
        ".asciz \"_ZNK7WebCore11MediaPlayer22hasAvailableVideoFrameEv\"\n"
        APS5_ASM_FUNCTION_END("_ZNK7WebCore11MediaPlayer22hasAvailableVideoFrameEv"));

__asm__(".text\n" APS5_ASM_FUNCTION("_ZNK7WebCore11MediaPlayer23hasSingleSecurityOriginEv")
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov 0x50(%rdi), %rdi\n"
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov (%rdi), %rax\n"
        "jmp *0x270(%rax)\n"
        "1:\n"
        "lea 2f(%rip), %rdi\n"
        "jmp " APS5_ASM_SYMBOL("NKWebKitNullDelegate_nid_no_patch") "\n"
        "2:\n"
        ".asciz \"_ZNK7WebCore11MediaPlayer23hasSingleSecurityOriginEv\"\n"
        APS5_ASM_FUNCTION_END("_ZNK7WebCore11MediaPlayer23hasSingleSecurityOriginEv"));

__asm__(".text\n" APS5_ASM_FUNCTION("_ZNK7WebCore11MediaPlayer24shouldUsePersistentCacheEv")
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov 0x18(%rdi), %rdi\n"
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov (%rdi), %rax\n"
        "jmp *0x110(%rax)\n"
        "1:\n"
        "lea 2f(%rip), %rdi\n"
        "jmp " APS5_ASM_SYMBOL("NKWebKitNullDelegate_nid_no_patch") "\n"
        "2:\n"
        ".asciz \"_ZNK7WebCore11MediaPlayer24shouldUsePersistentCacheEv\"\n"
        APS5_ASM_FUNCTION_END("_ZNK7WebCore11MediaPlayer24shouldUsePersistentCacheEv"));

__asm__(".text\n" APS5_ASM_FUNCTION("_ZNK7WebCore11MediaPlayer24supportsPictureInPictureEv")
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov 0x50(%rdi), %rdi\n"
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov (%rdi), %rax\n"
        "jmp *0x60(%rax)\n"
        "1:\n"
        "lea 2f(%rip), %rdi\n"
        "jmp " APS5_ASM_SYMBOL("NKWebKitNullDelegate_nid_no_patch") "\n"
        "2:\n"
        ".asciz \"_ZNK7WebCore11MediaPlayer24supportsPictureInPictureEv\"\n"
        APS5_ASM_FUNCTION_END("_ZNK7WebCore11MediaPlayer24supportsPictureInPictureEv"));

__asm__(".text\n" APS5_ASM_FUNCTION("_ZNK7WebCore11MediaPlayer25renderingCanBeAcceleratedEv")
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov 0x18(%rdi), %rdi\n"
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov (%rdi), %rax\n"
        "jmp *0x80(%rax)\n"
        "1:\n"
        "lea 2f(%rip), %rdi\n"
        "jmp " APS5_ASM_SYMBOL("NKWebKitNullDelegate_nid_no_patch") "\n"
        "2:\n"
        ".asciz \"_ZNK7WebCore11MediaPlayer25renderingCanBeAcceleratedEv\"\n"
        APS5_ASM_FUNCTION_END("_ZNK7WebCore11MediaPlayer25renderingCanBeAcceleratedEv"));

__asm__(".text\n" APS5_ASM_FUNCTION("_ZNK7WebCore11MediaPlayer26shouldCheckHardwareSupportEv")
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov 0x18(%rdi), %rdi\n"
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov (%rdi), %rax\n"
        "jmp *0x1b0(%rax)\n"
        "1:\n"
        "lea 2f(%rip), %rdi\n"
        "jmp " APS5_ASM_SYMBOL("NKWebKitNullDelegate_nid_no_patch") "\n"
        "2:\n"
        ".asciz \"_ZNK7WebCore11MediaPlayer26shouldCheckHardwareSupportEv\"\n"
        APS5_ASM_FUNCTION_END("_ZNK7WebCore11MediaPlayer26shouldCheckHardwareSupportEv"));

__asm__(".text\n" APS5_ASM_FUNCTION("_ZNK7WebCore11MediaPlayer28requiresImmediateCompositingEv")
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov 0x50(%rdi), %rdi\n"
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov (%rdi), %rax\n"
        "jmp *0x80(%rax)\n"
        "1:\n"
        "lea 2f(%rip), %rdi\n"
        "jmp " APS5_ASM_SYMBOL("NKWebKitNullDelegate_nid_no_patch") "\n"
        "2:\n"
        ".asciz \"_ZNK7WebCore11MediaPlayer28requiresImmediateCompositingEv\"\n"
        APS5_ASM_FUNCTION_END("_ZNK7WebCore11MediaPlayer28requiresImmediateCompositingEv"));

__asm__(".text\n" APS5_ASM_FUNCTION("_ZNK7WebCore11MediaPlayer28supportsAcceleratedRenderingEv")
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov 0x50(%rdi), %rdi\n"
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov (%rdi), %rax\n"
        "jmp *0x258(%rax)\n"
        "1:\n"
        "lea 2f(%rip), %rdi\n"
        "jmp " APS5_ASM_SYMBOL("NKWebKitNullDelegate_nid_no_patch") "\n"
        "2:\n"
        ".asciz \"_ZNK7WebCore11MediaPlayer28supportsAcceleratedRenderingEv\"\n"
        APS5_ASM_FUNCTION_END("_ZNK7WebCore11MediaPlayer28supportsAcceleratedRenderingEv"));

__asm__(".text\n" APS5_ASM_FUNCTION("_ZNK7WebCore11MediaPlayer31maximumDurationToCacheMediaTimeEv")
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov 0x50(%rdi), %rdi\n"
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov (%rdi), %rax\n"
        "jmp *0x2a0(%rax)\n"
        "1:\n"
        "lea 2f(%rip), %rdi\n"
        "jmp " APS5_ASM_SYMBOL("NKWebKitNullDelegate_nid_no_patch") "\n"
        "2:\n"
        ".asciz \"_ZNK7WebCore11MediaPlayer31maximumDurationToCacheMediaTimeEv\"\n"
        APS5_ASM_FUNCTION_END("_ZNK7WebCore11MediaPlayer31maximumDurationToCacheMediaTimeEv"));

__asm__(".text\n" APS5_ASM_FUNCTION("_ZNK7WebCore11MediaPlayer31requiresTextTrackRepresentationEv")
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov 0x50(%rdi), %rdi\n"
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov (%rdi), %rax\n"
        "jmp *0x2d8(%rax)\n"
        "1:\n"
        "lea 2f(%rip), %rdi\n"
        "jmp " APS5_ASM_SYMBOL("NKWebKitNullDelegate_nid_no_patch") "\n"
        "2:\n"
        ".asciz \"_ZNK7WebCore11MediaPlayer31requiresTextTrackRepresentationEv\"\n"
        APS5_ASM_FUNCTION_END("_ZNK7WebCore11MediaPlayer31requiresTextTrackRepresentationEv"));

__asm__(".text\n" APS5_ASM_FUNCTION("_ZNK7WebCore11MediaPlayer35platformVolumeConfigurationRequiredEv")
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov 0x18(%rdi), %rdi\n"
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov (%rdi), %rax\n"
        "jmp *0xe8(%rax)\n"
        "1:\n"
        "lea 2f(%rip), %rdi\n"
        "jmp " APS5_ASM_SYMBOL("NKWebKitNullDelegate_nid_no_patch") "\n"
        "2:\n"
        ".asciz \"_ZNK7WebCore11MediaPlayer35platformVolumeConfigurationRequiredEv\"\n"
        APS5_ASM_FUNCTION_END("_ZNK7WebCore11MediaPlayer35platformVolumeConfigurationRequiredEv"));

__asm__(".text\n" APS5_ASM_FUNCTION("_ZNK7WebCore11MediaPlayer41mediaContentTypesRequiringHardwareSupportEv")
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov 0x18(%rdi), %rdi\n"
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov (%rdi), %rax\n"
        "jmp *0x1a8(%rax)\n"
        "1:\n"
        "lea 2f(%rip), %rdi\n"
        "jmp " APS5_ASM_SYMBOL("NKWebKitNullDelegate_nid_no_patch") "\n"
        "2:\n"
        ".asciz \"_ZNK7WebCore11MediaPlayer41mediaContentTypesRequiringHardwareSupportEv\"\n"
        APS5_ASM_FUNCTION_END("_ZNK7WebCore11MediaPlayer41mediaContentTypesRequiringHardwareSupportEv"));

__asm__(".text\n" APS5_ASM_FUNCTION("_ZNK7WebCore11MediaPlayer4rateEv")
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov 0x50(%rdi), %rdi\n"
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov (%rdi), %rax\n"
        "jmp *0x148(%rax)\n"
        "1:\n"
        "lea 2f(%rip), %rdi\n"
        "jmp " APS5_ASM_SYMBOL("NKWebKitNullDelegate_nid_no_patch") "\n"
        "2:\n"
        ".asciz \"_ZNK7WebCore11MediaPlayer4rateEv\"\n"
        APS5_ASM_FUNCTION_END("_ZNK7WebCore11MediaPlayer4rateEv"));

__asm__(".text\n" APS5_ASM_FUNCTION("_ZNK7WebCore11MediaPlayer5endedEv")
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov 0x50(%rdi), %rdi\n"
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov (%rdi), %rax\n"
        "jmp *0x320(%rax)\n"
        "1:\n"
        "lea 2f(%rip), %rdi\n"
        "jmp " APS5_ASM_SYMBOL("NKWebKitNullDelegate_nid_no_patch") "\n"
        "2:\n"
        ".asciz \"_ZNK7WebCore11MediaPlayer5endedEv\"\n"
        APS5_ASM_FUNCTION_END("_ZNK7WebCore11MediaPlayer5endedEv"));

__asm__(".text\n" APS5_ASM_FUNCTION("_ZNK7WebCore11MediaPlayer6pausedEv")
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov 0x50(%rdi), %rdi\n"
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov (%rdi), %rax\n"
        "jmp *0x168(%rax)\n"
        "1:\n"
        "lea 2f(%rip), %rdi\n"
        "jmp " APS5_ASM_SYMBOL("NKWebKitNullDelegate_nid_no_patch") "\n"
        "2:\n"
        ".asciz \"_ZNK7WebCore11MediaPlayer6pausedEv\"\n"
        APS5_ASM_FUNCTION_END("_ZNK7WebCore11MediaPlayer6pausedEv"));

__asm__(".text\n" APS5_ASM_FUNCTION("_ZNK7WebCore11MediaPlayer7seekingEv")
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov 0x50(%rdi), %rdi\n"
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov (%rdi), %rax\n"
        "jmp *0x120(%rax)\n"
        "1:\n"
        "lea 2f(%rip), %rdi\n"
        "jmp " APS5_ASM_SYMBOL("NKWebKitNullDelegate_nid_no_patch") "\n"
        "2:\n"
        ".asciz \"_ZNK7WebCore11MediaPlayer7seekingEv\"\n"
        APS5_ASM_FUNCTION_END("_ZNK7WebCore11MediaPlayer7seekingEv"));

__asm__(".text\n" APS5_ASM_FUNCTION("_ZNK7WebCore11MediaPlayer8hasAudioEv")
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov 0x50(%rdi), %rdi\n"
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov (%rdi), %rax\n"
        "jmp *0xa0(%rax)\n"
        "1:\n"
        "lea 2f(%rip), %rdi\n"
        "jmp " APS5_ASM_SYMBOL("NKWebKitNullDelegate_nid_no_patch") "\n"
        "2:\n"
        ".asciz \"_ZNK7WebCore11MediaPlayer8hasAudioEv\"\n"
        APS5_ASM_FUNCTION_END("_ZNK7WebCore11MediaPlayer8hasAudioEv"));

__asm__(".text\n" APS5_ASM_FUNCTION("_ZNK7WebCore11MediaPlayer8hasVideoEv")
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov 0x50(%rdi), %rdi\n"
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov (%rdi), %rax\n"
        "jmp *0x98(%rax)\n"
        "1:\n"
        "lea 2f(%rip), %rdi\n"
        "jmp " APS5_ASM_SYMBOL("NKWebKitNullDelegate_nid_no_patch") "\n"
        "2:\n"
        ".asciz \"_ZNK7WebCore11MediaPlayer8hasVideoEv\"\n"
        APS5_ASM_FUNCTION_END("_ZNK7WebCore11MediaPlayer8hasVideoEv"));

__asm__(".text\n" APS5_ASM_FUNCTION("_ZNK7WebCore11MediaPlayer9isLoopingEv")
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov 0x18(%rdi), %rdi\n"
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov (%rdi), %rax\n"
        "jmp *0xf0(%rax)\n"
        "1:\n"
        "lea 2f(%rip), %rdi\n"
        "jmp " APS5_ASM_SYMBOL("NKWebKitNullDelegate_nid_no_patch") "\n"
        "2:\n"
        ".asciz \"_ZNK7WebCore11MediaPlayer9isLoopingEv\"\n"
        APS5_ASM_FUNCTION_END("_ZNK7WebCore11MediaPlayer9isLoopingEv"));

__asm__(".text\n" APS5_ASM_FUNCTION("_ZNK7WebCore16HTMLInputElement11valueAsDateEv")
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov 0xf8(%rdi), %rdi\n"
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov (%rdi), %rax\n"
        "jmp *0x58(%rax)\n"
        "1:\n"
        "lea 2f(%rip), %rdi\n"
        "jmp " APS5_ASM_SYMBOL("NKWebKitNullDelegate_nid_no_patch") "\n"
        "2:\n"
        ".asciz \"_ZNK7WebCore16HTMLInputElement11valueAsDateEv\"\n"
        APS5_ASM_FUNCTION_END("_ZNK7WebCore16HTMLInputElement11valueAsDateEv"));

__asm__(".text\n" APS5_ASM_FUNCTION("_ZNK7WebCore16HTMLInputElement13valueAsNumberEv")
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov 0xf8(%rdi), %rdi\n"
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov (%rdi), %rax\n"
        "jmp *0x68(%rax)\n"
        "1:\n"
        "lea 2f(%rip), %rdi\n"
        "jmp " APS5_ASM_SYMBOL("NKWebKitNullDelegate_nid_no_patch") "\n"
        "2:\n"
        ".asciz \"_ZNK7WebCore16HTMLInputElement13valueAsNumberEv\"\n"
        APS5_ASM_FUNCTION_END("_ZNK7WebCore16HTMLInputElement13valueAsNumberEv"));

__asm__(".text\n" APS5_ASM_FUNCTION("_ZNK7WebCore16HTMLInputElement21autoFillButtonElementEv")
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov 0xf8(%rdi), %rdi\n"
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov (%rdi), %rax\n"
        "jmp *0x218(%rax)\n"
        "1:\n"
        "lea 2f(%rip), %rdi\n"
        "jmp " APS5_ASM_SYMBOL("NKWebKitNullDelegate_nid_no_patch") "\n"
        "2:\n"
        ".asciz \"_ZNK7WebCore16HTMLInputElement21autoFillButtonElementEv\"\n"
        APS5_ASM_FUNCTION_END("_ZNK7WebCore16HTMLInputElement21autoFillButtonElementEv"));

__asm__(".text\n" APS5_ASM_FUNCTION("_ZNK7WebCore16HTMLInputElement5widthEv")
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov 0xf8(%rdi), %rdi\n"
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov (%rdi), %rax\n"
        "jmp *0x370(%rax)\n"
        "1:\n"
        "lea 2f(%rip), %rdi\n"
        "jmp " APS5_ASM_SYMBOL("NKWebKitNullDelegate_nid_no_patch") "\n"
        "2:\n"
        ".asciz \"_ZNK7WebCore16HTMLInputElement5widthEv\"\n"
        APS5_ASM_FUNCTION_END("_ZNK7WebCore16HTMLInputElement5widthEv"));

__asm__(".text\n" APS5_ASM_FUNCTION("_ZNK7WebCore16HTMLInputElement6heightEv")
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov 0xf8(%rdi), %rdi\n"
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov (%rdi), %rax\n"
        "jmp *0x368(%rax)\n"
        "1:\n"
        "lea 2f(%rip), %rdi\n"
        "jmp " APS5_ASM_SYMBOL("NKWebKitNullDelegate_nid_no_patch") "\n"
        "2:\n"
        ".asciz \"_ZNK7WebCore16HTMLInputElement6heightEv\"\n"
        APS5_ASM_FUNCTION_END("_ZNK7WebCore16HTMLInputElement6heightEv"));

__asm__(".text\n" APS5_ASM_FUNCTION("_ZNK7WebCore18RenderLayerBacking12tiledBackingEv")
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov 0x38(%rdi), %rdi\n"
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov (%rdi), %rax\n"
        "jmp *0x2e0(%rax)\n"
        "1:\n"
        "lea 2f(%rip), %rdi\n"
        "jmp " APS5_ASM_SYMBOL("NKWebKitNullDelegate_nid_no_patch") "\n"
        "2:\n"
        ".asciz \"_ZNK7WebCore18RenderLayerBacking12tiledBackingEv\"\n"
        APS5_ASM_FUNCTION_END("_ZNK7WebCore18RenderLayerBacking12tiledBackingEv"));

__asm__(".text\n" APS5_ASM_FUNCTION("_ZNK7WebCore21BackForwardController12forwardCountEv")
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov 0x8(%rdi), %rdi\n"
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov (%rdi), %rax\n"
        "jmp *0x30(%rax)\n"
        "1:\n"
        "lea 2f(%rip), %rdi\n"
        "jmp " APS5_ASM_SYMBOL("NKWebKitNullDelegate_nid_no_patch") "\n"
        "2:\n"
        ".asciz \"_ZNK7WebCore21BackForwardController12forwardCountEv\"\n"
        APS5_ASM_FUNCTION_END("_ZNK7WebCore21BackForwardController12forwardCountEv"));

__asm__(".text\n" APS5_ASM_FUNCTION("_ZNK7WebCore21BackForwardController9backCountEv")
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov 0x8(%rdi), %rdi\n"
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov (%rdi), %rax\n"
        "jmp *0x28(%rax)\n"
        "1:\n"
        "lea 2f(%rip), %rdi\n"
        "jmp " APS5_ASM_SYMBOL("NKWebKitNullDelegate_nid_no_patch") "\n"
        "2:\n"
        ".asciz \"_ZNK7WebCore21BackForwardController9backCountEv\"\n"
        APS5_ASM_FUNCTION_END("_ZNK7WebCore21BackForwardController9backCountEv"));

__asm__(".text\n" APS5_ASM_FUNCTION("_ZNK7WebCore6Chrome13setWindowRectERKNS_9FloatRectE")
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov 0x10(%rdi), %rdi\n"
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov (%rdi), %rax\n"
        "jmp *0x8(%rax)\n"
        "1:\n"
        "lea 2f(%rip), %rdi\n"
        "jmp " APS5_ASM_SYMBOL("NKWebKitNullDelegate_nid_no_patch") "\n"
        "2:\n"
        ".asciz \"_ZNK7WebCore6Chrome13setWindowRectERKNS_9FloatRectE\"\n"
        APS5_ASM_FUNCTION_END("_ZNK7WebCore6Chrome13setWindowRectERKNS_9FloatRectE"));

__asm__(".text\n" APS5_ASM_FUNCTION("_ZNK7WebCore6Chrome4showEv")
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov 0x10(%rdi), %rdi\n"
        "test %rdi, %rdi\n"
        "jz 1f\n"
        "mov (%rdi), %rax\n"
        "jmp *0x58(%rax)\n"
        "1:\n"
        "lea 2f(%rip), %rdi\n"
        "jmp " APS5_ASM_SYMBOL("NKWebKitNullDelegate_nid_no_patch") "\n"
        "2:\n"
        ".asciz \"_ZNK7WebCore6Chrome4showEv\"\n"
        APS5_ASM_FUNCTION_END("_ZNK7WebCore6Chrome4showEv"));
