#pragma once

#include <memory>
#include <string>

struct AVIOContext;

namespace AvPlayer {

class HttpStream {
public:
    HttpStream();
    ~HttpStream();
    bool Open(const std::string& url);
    AVIOContext* Context() const;
    void Rethrow() const;
    void Start();
    void Stop();
private:
    struct State;
    std::unique_ptr<State> state;
};

}
