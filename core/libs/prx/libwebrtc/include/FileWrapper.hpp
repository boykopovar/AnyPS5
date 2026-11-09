#pragma once

struct FileStream;

struct WebRtcFileWrapper {
    FileStream* file;
};

static_assert(sizeof(WebRtcFileWrapper) == 8);
