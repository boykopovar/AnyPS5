#include "prx/libSceAvPlayer/include/HttpStream.hpp"
#include "prx/libSceHttp/include/Client.hpp"
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <exception>

extern "C" {
#include <libavformat/avio.h>
#include <libavutil/error.h>
#include <libavutil/mem.h>
}

namespace AvPlayer {

struct HttpStream::State {
    Aps5HttpClient* client = Aps5HttpClient_Create_nid_postfix();
    AVIOContext* context = nullptr;
    std::exception_ptr failure;

    ~State() {
        if (context) {
            av_freep(&context->buffer);
            avio_context_free(&context);
        }
        failure = nullptr;
        Aps5HttpClient_Destroy_nid_postfix(client);
    }

    static int Read(void* opaque, std::uint8_t* destination, int capacity) {
        auto& state = *static_cast<State*>(opaque);
        try {
            const auto count = Aps5HttpClient_Read_nid_postfix(state.client, destination, capacity);
            return count == 0 ? AVERROR_EOF : count;
        } catch (...) {
            state.failure = std::current_exception();
            return AVERROR(EIO);
        }
    }

    static std::int64_t Seek(void* opaque, std::int64_t offset, int whence) {
        auto& state = *static_cast<State*>(opaque);
        try {
            if (whence == AVSEEK_SIZE) {
                const auto size = Aps5HttpClient_GetSize_nid_postfix(state.client);
                return size < 0 ? AVERROR(ENOSYS) : size;
            }
            whence &= ~AVSEEK_FORCE;
            std::int32_t origin;
            if (whence == SEEK_SET) origin = 0;
            else if (whence == SEEK_CUR) origin = 1;
            else if (whence == SEEK_END) origin = 2;
            else return AVERROR(EINVAL);
            const auto position = Aps5HttpClient_Seek_nid_postfix(state.client, offset, origin);
            return position < 0 ? AVERROR(EINVAL) : position;
        } catch (...) {
            state.failure = std::current_exception();
            return AVERROR(EIO);
        }
    }
};

HttpStream::HttpStream() : state(std::make_unique<State>()) {}
HttpStream::~HttpStream() = default;

bool HttpStream::Open(const std::string& url) {
    if (state->context) {
        av_freep(&state->context->buffer);
        avio_context_free(&state->context);
    }
    state->failure = nullptr;
    if (url.find('\0') != std::string::npos) return false;
    if (Aps5HttpClient_Open_nid_postfix(state->client, url.c_str()) == 0) return false;
    auto* buffer = static_cast<std::uint8_t*>(av_malloc(65536));
    if (!buffer) return false;
    state->context = avio_alloc_context(buffer, 65536, 0, state.get(), State::Read, nullptr, State::Seek);
    if (!state->context) av_free(buffer);
    return state->context != nullptr;
}

AVIOContext* HttpStream::Context() const { return state->context; }
void HttpStream::Rethrow() const { if (state->failure) std::rethrow_exception(state->failure); }
void HttpStream::Start() {
    state->failure = nullptr;
    Aps5HttpClient_Start_nid_postfix(state->client);
    if (state->context) {
        state->context->error = 0;
        state->context->eof_reached = 0;
    }
}
void HttpStream::Stop() { Aps5HttpClient_Stop_nid_postfix(state->client); }

}
