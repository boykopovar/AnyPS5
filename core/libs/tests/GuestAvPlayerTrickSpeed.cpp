#define main GuestAvPlayerAllTestsMain
#include "GuestAvPlayer.cpp"
#undef main

namespace {

void WriteWebmMovie() {
    std::ofstream file(WebmPath, std::ios::binary | std::ios::trunc);
    file.write(reinterpret_cast<const char*>(WebmMovie.data()), static_cast<std::streamsize>(WebmMovie.size()));
    Check(static_cast<bool>(file), "cannot write the WebM movie");
}

}

int main() {
    try {
        WriteMovie();
        WriteWebmMovie();
        std::filesystem::create_directories("app0/archive.mp4");
        std::filesystem::copy_file(MoviePath, "app0/archive.mp4/movie", std::filesystem::copy_options::overwrite_existing);
        std::filesystem::copy_file(MoviePath, "app0/avplayer.unknown", std::filesystem::copy_options::overwrite_existing);
        std::filesystem::copy_file(MoviePath, "app0/avplayer.m3d", std::filesystem::copy_options::overwrite_existing);
        auto init = InitData(nullptr);
        auto* player = sceAvPlayerInit(&init);
        Check(player != nullptr, "AVPlayer initialization failed");
        Check(sceAvPlayerSetTrickSpeed(player, 0) == OperationFailed,
              "missing source did not take precedence over zero speed");
        Check(sceAvPlayerSetTrickSpeed(player, 1) == OperationFailed,
              "missing source did not take precedence over unsupported speed");
        Check(sceAvPlayerAddSource(player, "/app0/avplayer.mp4") == 0,
              "MP4 source initialization failed");
        for (const auto rejected : {-399, -1, 0, 1, 99, 101, 399}) {
            Check(sceAvPlayerSetTrickSpeed(player, rejected) == AvPlayer::SCE_AVPLAYER_ERROR_NOT_SUPPORTED,
                  "unsupported trick speed was accepted: " + std::to_string(rejected));
        }
        for (const auto accepted : {-400, 100, 400}) {
            Check(sceAvPlayerSetTrickSpeed(player, accepted) == AvPlayer::SCE_OK,
                  "supported MP4 trick speed was rejected: " + std::to_string(accepted));
            Check(AvPlayer::GetAvPlayerTrickSpeedForTest_nid_no_patch(player) == accepted,
                  "supported trick speed was not preserved: " + std::to_string(accepted));
        }
        Check(sceAvPlayerSetTrickSpeed(player, std::numeric_limits<std::int32_t>::max()) == AvPlayer::SCE_OK,
              "large positive trick speed was rejected");
        Check(AvPlayer::GetAvPlayerTrickSpeedForTest_nid_no_patch(player) == 3200,
              "positive trick speed was not clamped to 3200");
        Check(sceAvPlayerSetTrickSpeed(player, std::numeric_limits<std::int32_t>::min()) == AvPlayer::SCE_OK,
              "large negative trick speed was rejected");
        Check(AvPlayer::GetAvPlayerTrickSpeedForTest_nid_no_patch(player) == -3200,
              "negative trick speed was not clamped to -3200");
        Check(sceAvPlayerClose(player) == 0, "MP4 player close failed");

        for (const auto* path : {"/app0/avplayer.unknown", "/app0/archive.mp4/movie"}) {
            init = InitData(nullptr);
            player = sceAvPlayerInit(&init);
            Check(player != nullptr, "unknown-source player initialization failed");
            Check(sceAvPlayerAddSource(player, path) == 0, std::string("could not open movie: ") + path);
            Check(sceAvPlayerSetTrickSpeed(player, 400) == AvPlayer::SCE_AVPLAYER_ERROR_NOT_SUPPORTED,
                  std::string("trick speed was accepted for an unknown source type: ") + path);
            Check(sceAvPlayerClose(player) == 0, "unknown-source player close failed");
        }

        init = InitData(nullptr);
        player = sceAvPlayerInit(&init);
        Check(player != nullptr, "M3D player initialization failed");
        Check(sceAvPlayerAddSource(player, "/app0/avplayer.m3d") == 0,
              "M3D media source could not be opened");
        Check(sceAvPlayerSetTrickSpeed(player, 400) == AvPlayer::SCE_AVPLAYER_ERROR_NOT_SUPPORTED,
              "trick speed was accepted for the unsupported M3D extension");
        Check(sceAvPlayerClose(player) == 0, "M3D player close failed");

        init = InitData(nullptr);
        player = sceAvPlayerInit(&init);
        Check(player != nullptr, "WebM player initialization failed");
        Check(sceAvPlayerAddSource(player, "/app0/avplayer.webm") == 0,
              "WebM source initialization failed");
        Check(sceAvPlayerSetTrickSpeed(player, 400) == AvPlayer::SCE_OK,
              "trick speed was rejected for WebM source type 2");
        Check(sceAvPlayerClose(player) == 0, "WebM player close failed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
