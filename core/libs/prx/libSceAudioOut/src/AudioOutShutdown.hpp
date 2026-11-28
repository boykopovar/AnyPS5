#ifndef CORE_LIBS_PRX_LIBSCEAUDIOOUT_SRC_AUDIOOUTSHUTDOWN_HPP
#define CORE_LIBS_PRX_LIBSCEAUDIOOUT_SRC_AUDIOOUTSHUTDOWN_HPP

#include <cstdint>

class AudioOutCall final {
public:
    AudioOutCall();
    ~AudioOutCall();
    AudioOutCall(const AudioOutCall&) = delete;
    AudioOutCall& operator=(const AudioOutCall&) = delete;
    static std::uint32_t ActiveCount();
    static std::uint32_t ThreadDepth();
};

#endif
