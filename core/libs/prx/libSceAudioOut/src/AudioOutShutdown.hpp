#ifndef CORE_LIBS_PRX_LIBSCEAUDIOOUT_SRC_AUDIOOUTSHUTDOWN_HPP
#define CORE_LIBS_PRX_LIBSCEAUDIOOUT_SRC_AUDIOOUTSHUTDOWN_HPP

class AudioOutCall final {
public:
    AudioOutCall();
    ~AudioOutCall();
    AudioOutCall(const AudioOutCall&) = delete;
    AudioOutCall& operator=(const AudioOutCall&) = delete;
};

#endif
