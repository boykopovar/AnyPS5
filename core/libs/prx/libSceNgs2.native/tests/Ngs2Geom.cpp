#include "Ngs2Test.hpp"

#include <cstdint>
#include <cstring>
#include <cmath>
#include <limits>
#include <stdexcept>

static constexpr float SoundSpeed = 343.0f;

static void TestLayouts() {
    Require(sizeof(Ngs2GeomVector) == 12);
    Require(sizeof(Ngs2GeomCone) == 16);
    Require(sizeof(Ngs2GeomRolloff) == 16);
    Require(sizeof(Ngs2GeomListenerParam) == 60);
    Require(sizeof(Ngs2GeomListenerWork) == 96);
    Require(sizeof(Ngs2GeomSourceParam) == 108);
    Require(sizeof(Ngs2GeomA3dAttribute) == 32);
    Require(sizeof(Ngs2GeomAttribute) == 308);
}

static void TestResetListenerParam() {
    Ngs2GeomListenerParam param;
    std::memset(&param, 0x5a, sizeof(param));
    Require(sceNgs2GeomResetListenerParam(&param) == SCE_NGS2_OK);
    Require(param.position.x == 0.0f && param.position.y == 0.0f && param.position.z == 0.0f);
    Require(param.velocity.x == 0.0f && param.velocity.y == 0.0f && param.velocity.z == 0.0f);
    Require(param.orient_front.x == 0.0f && param.orient_front.y == 0.0f && param.orient_front.z == 1.0f);
    Require(param.orient_up.x == 0.0f && param.orient_up.y == 1.0f && param.orient_up.z == 0.0f);
    Require(param.sound_speed == SoundSpeed);
    Require(param.reserved[0] == 0u && param.reserved[1] == 0u);
    Require(sceNgs2GeomResetListenerParam(nullptr) == SCE_NGS2_ERROR_INVALID_OUT_ADDRESS);
}

static void TestResetSourceParam() {
    Ngs2GeomSourceParam param;
    std::memset(&param, 0x5a, sizeof(param));
    Require(sceNgs2GeomResetSourceParam(&param) == SCE_NGS2_OK);
    Require(param.position.x == 0.0f && param.position.y == 0.0f && param.position.z == 0.0f);
    Require(param.velocity.x == 0.0f && param.velocity.y == 0.0f && param.velocity.z == 0.0f);
    Require(param.direction.x == 0.0f && param.direction.y == 0.0f && param.direction.z == 1.0f);
    Require(param.cone.inner_level == 1.0f && param.cone.inner_angle == 360.0f);
    Require(param.cone.outer_level == 1.0f && param.cone.outer_angle == 360.0f);
    Require(param.rolloff.model == 0u);
    Require(param.rolloff.max_distance == 1000000.0f);
    Require(param.rolloff.rolloff_factor == 1.0f);
    Require(param.rolloff.reference_distance == 1.0f);
    Require(param.doppler_factor == 1.0f && param.fbw_level == 1.0f && param.lfe_level == 1.0f);
    Require(param.max_level == 1.0f && param.min_level == 0.0f && param.radius == 0.0f);
    Require(param.num_speakers == 2u && param.matrix_format == 2u);
    Require(param.reserved[0] == 0u && param.reserved[1] == 0u);
    Require(sceNgs2GeomResetSourceParam(nullptr) == SCE_NGS2_ERROR_INVALID_OUT_ADDRESS);
}

static Ngs2GeomListenerParam Listener() {
    Ngs2GeomListenerParam param{};
    Require(sceNgs2GeomResetListenerParam(&param) == SCE_NGS2_OK);
    return param;
}

static void RequireMatrix(const Ngs2GeomListenerWork& work, const float (&expected)[4][4]) {
    for (std::size_t row = 0; row < 4; row++) {
        for (std::size_t column = 0; column < 4; column++) Require(std::abs(work.matrix[row][column] - expected[row][column]) < 0.00001f);
    }
}

static void TestListenerTransform() {
    auto param = Listener();
    param.velocity = {2.0f, -3.0f, 4.0f};
    param.sound_speed = 1500.0f;
    Ngs2GeomListenerWork work;
    std::memset(&work, 0x5a, sizeof(work));
    Require(sceNgs2GeomCalcListener(&param, &work, 0) == SCE_NGS2_OK);
    RequireMatrix(work, {{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}});
    Require(work.velocity.x == 2.0f && work.velocity.y == -3.0f && work.velocity.z == 4.0f);
    Require(work.sound_speed == 1500.0f && work.coordinate == 0);
    Require(work.reserved[0] == 0 && work.reserved[1] == 0 && work.reserved[2] == 0);

    param.position = {3.0f, 4.0f, 5.0f};
    Require(sceNgs2GeomCalcListener(&param, &work, 0) == SCE_NGS2_OK);
    RequireMatrix(work, {{1, 0, 0, -3}, {0, 1, 0, -4}, {0, 0, 1, -5}, {0, 0, 0, 1}});

    param.orient_front = {2.0f, 0.0f, 0.0f};
    param.orient_up = {1.0f, 3.0f, 0.0f};
    Require(sceNgs2GeomCalcListener(&param, &work, 0) == SCE_NGS2_OK);
    RequireMatrix(work, {{0, 0, -1, 5}, {0, 1, 0, -4}, {1, 0, 0, -3}, {0, 0, 0, 1}});

    param.orient_front = {0.0f, 0.0f, -2.0f};
    param.orient_up = {0.0f, 3.0f, 0.0f};
    Require(sceNgs2GeomCalcListener(&param, &work, 1) == SCE_NGS2_OK);
    RequireMatrix(work, {{1, 0, 0, -3}, {0, 1, 0, -4}, {0, 0, -1, 5}, {0, 0, 0, 1}});
    Require(work.coordinate == 1);

    param.orient_front = {0.0f, 0.0f, std::numeric_limits<float>::max()};
    param.orient_up = {0.0f, std::numeric_limits<float>::min(), 0.0f};
    Require(sceNgs2GeomCalcListener(&param, &work, 0) == SCE_NGS2_OK);
    RequireMatrix(work, {{1, 0, 0, -3}, {0, 1, 0, -4}, {0, 0, 1, -5}, {0, 0, 0, 1}});
}

static void RejectListener(const Ngs2GeomListenerParam* param, std::uint32_t flags) {
    Ngs2GeomListenerWork work;
    std::memset(&work, 0x5a, sizeof(work));
    unsigned char before[sizeof(work)];
    std::memcpy(before, &work, sizeof(work));
    bool rejected = false;
    try {
        sceNgs2GeomCalcListener(param, &work, flags);
    } catch (const std::exception&) {
        rejected = true;
    }
    Require(rejected);
    Require(std::memcmp(before, &work, sizeof(work)) == 0);
}

static void TestInvalidListener() {
    auto param = Listener();
    Require(sceNgs2GeomCalcListener(&param, nullptr, 0) == SCE_NGS2_ERROR_INVALID_OUT_ADDRESS);
    RejectListener(nullptr, 0);
    RejectListener(&param, 2);
    RejectListener(&param, 0xffffffffu);
    for (const float value : {0.0f, -1.0f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()}) {
        param.sound_speed = value;
        RejectListener(&param, 0);
    }
    param = Listener();
    param.orient_front = {};
    RejectListener(&param, 0);
    param = Listener();
    param.orient_up = param.orient_front;
    RejectListener(&param, 0);
    param = Listener();
    param.orient_up = {};
    RejectListener(&param, 0);
    param = Listener();
    param.position.x = std::numeric_limits<float>::infinity();
    RejectListener(&param, 0);
    param = Listener();
    param.velocity.z = std::numeric_limits<float>::quiet_NaN();
    RejectListener(&param, 0);
    param = Listener();
    param.orient_front.y = std::numeric_limits<float>::quiet_NaN();
    RejectListener(&param, 0);
}

static Ngs2GeomSourceParam Source() {
    Ngs2GeomSourceParam param{};
    Require(sceNgs2GeomResetSourceParam(&param) == SCE_NGS2_OK);
    return param;
}

static Ngs2GeomListenerWork ListenerWork(std::uint32_t flags = 0) {
    auto param = Listener();
    Ngs2GeomListenerWork work{};
    Require(sceNgs2GeomCalcListener(&param, &work, flags) == SCE_NGS2_OK);
    return work;
}

static bool Near(float left, float right, float tolerance = 0.00001f) {
    return std::abs(left - right) < tolerance;
}

static void RequireZero(const Ngs2GeomAttribute& attribute, std::uint32_t first, std::uint32_t count) {
    for (std::uint32_t index = first; index < first + count; index++) Require(attribute.level[index] == 0.0f);
}

static void TestApplyStereo() {
    const auto work = ListenerWork();
    auto source = Source();
    Ngs2GeomAttribute attribute;
    std::memset(&attribute, 0x5a, sizeof(attribute));

    source.position = {0.0f, 0.0f, 1.0f};
    Require(sceNgs2GeomApply(&work, &source, &attribute, 0) == SCE_NGS2_OK);
    Require(attribute.pitch_ratio == 1.0f);
    Require(Near(attribute.level[0], attribute.level[1]));
    Require(Near(attribute.level[0], 0.7071068f));
    Require(Near(attribute.level[0] * attribute.level[0] + attribute.level[1] * attribute.level[1], 1.0f));
    Require(Near(attribute.level[8], attribute.level[0]));
    Require(Near(attribute.level[9], attribute.level[1]));
    RequireZero(attribute, 2, 6);
    RequireZero(attribute, 10, 54);
    Require(attribute.a3d_attrib.position.x == 0.0f && attribute.a3d_attrib.position.y == 0.0f && attribute.a3d_attrib.position.z == 1.0f);
    Require(Near(attribute.a3d_attrib.volume, 1.0f));
    Require(attribute.reserved[0] == 0u && attribute.reserved[3] == 0u);

    source.position = {1.0f, 0.0f, 0.0f};
    Require(sceNgs2GeomApply(&work, &source, &attribute, 0) == SCE_NGS2_OK);
    Require(Near(attribute.level[0], 0.0f));
    Require(Near(attribute.level[1], 1.0f));
    Require(Near(attribute.a3d_attrib.position.x, 1.0f));

    source.position = {0.0f, 5.0f, 0.0f};
    Require(sceNgs2GeomApply(&work, &source, &attribute, 0) == SCE_NGS2_OK);
    Require(Near(attribute.level[0], attribute.level[1]));

    source.position = {1.0f, 0.0f, 0.0f};
    source.radius = 2.0f;
    Require(sceNgs2GeomApply(&work, &source, &attribute, 0) == SCE_NGS2_OK);
    Require(Near(attribute.level[0], 0.7071068f));
    Require(Near(attribute.level[1], 0.7071068f));
}

static void TestApplySurround() {
    const auto work = ListenerWork();
    auto source = Source();
    source.num_speakers = 8;
    source.lfe_level = 0.5f;
    Ngs2GeomAttribute attribute;

    source.position = {1.0f, 0.0f, 0.0f};
    Require(sceNgs2GeomApply(&work, &source, &attribute, 0) == SCE_NGS2_OK);
    Require(Near(attribute.level[3], 0.5f));
    Require(Near(attribute.level[5], 1.0f));
    Require(Near(attribute.level[8 + 3], 0.5f));
    Require(Near(attribute.level[8 + 5], 1.0f));
    for (const std::uint32_t index : {0u, 1u, 2u, 4u, 6u, 7u}) Require(Near(attribute.level[index], 0.0f));

    source.position = {-0.7071068f, 0.0f, -0.7071068f};
    Require(sceNgs2GeomApply(&work, &source, &attribute, 0) == SCE_NGS2_OK);
    const float left_surround = attribute.level[4];
    const float left_back = attribute.level[6];
    Require(left_surround > 0.0f && left_back > 0.0f && left_back > left_surround);
    Require(Near(left_surround * left_surround + left_back * left_back, 1.0f));
}

static void TestApplyDistanceConeDoppler() {
    const auto work = ListenerWork();
    auto source = Source();
    source.position = {0.0f, 0.0f, 10.0f};
    source.cone = {1.0f, 90.0f, 0.25f, 180.0f};
    source.rolloff.model = 1;
    source.rolloff.max_distance = 11.0f;
    source.rolloff.reference_distance = 1.0f;
    Ngs2GeomAttribute attribute;
    Require(sceNgs2GeomApply(&work, &source, &attribute, 0) == SCE_NGS2_OK);
    Require(Near(attribute.a3d_attrib.volume, 0.025f));
    Require(Near(attribute.level[0], attribute.level[1]));

    source.min_level = 0.5f;
    Require(sceNgs2GeomApply(&work, &source, &attribute, 0) == SCE_NGS2_OK);
    Require(Near(attribute.a3d_attrib.volume, 0.5f));

    auto doppler = Source();
    doppler.position = {0.0f, 0.0f, 10.0f};
    doppler.velocity = {0.0f, 0.0f, -34.3f};
    Require(sceNgs2GeomApply(&work, &doppler, &attribute, 0) == SCE_NGS2_OK);
    Require(Near(attribute.pitch_ratio, 1.1111111f));

    doppler.doppler_factor = 0.0f;
    Require(sceNgs2GeomApply(&work, &doppler, &attribute, 0) == SCE_NGS2_OK);
    Require(Near(attribute.pitch_ratio, 1.0f));
}

template <typename Call>
static bool Throws(Call call) {
    try {
        call();
    } catch (const std::exception&) {
        return true;
    }
    return false;
}

static void RejectApply(const Ngs2GeomListenerWork* work, const Ngs2GeomSourceParam* source, std::uint32_t flags) {
    Ngs2GeomAttribute attribute;
    std::memset(&attribute, 0x5a, sizeof(attribute));
    unsigned char before[sizeof(attribute)];
    std::memcpy(before, &attribute, sizeof(attribute));
    Require(Throws([&] { sceNgs2GeomApply(work, source, &attribute, flags); }));
    Require(std::memcmp(before, &attribute, sizeof(attribute)) == 0);
}

static void TestInvalidApply() {
    const auto work = ListenerWork();
    auto source = Source();
    Require(sceNgs2GeomApply(&work, &source, nullptr, 0) == SCE_NGS2_ERROR_INVALID_OUT_ADDRESS);
    RejectApply(nullptr, &source, 0);
    RejectApply(&work, nullptr, 0);
    RejectApply(&work, &source, 0x20u);
    RejectApply(&work, &source, 0x100u);
    RejectApply(&work, &source, 0xffffffffu);
    source.position.x = std::numeric_limits<float>::infinity();
    RejectApply(&work, &source, 0);
    source = Source();
    source.fbw_level = std::numeric_limits<float>::quiet_NaN();
    RejectApply(&work, &source, 0);
}

static void TestApplyFlags() {
    const auto work = ListenerWork();
    auto source = Source();
    source.position = {0.0f, 0.0f, 10.0f};
    source.velocity = {0.0f, 0.0f, -34.3f};
    Ngs2GeomAttribute attribute;

    Require(sceNgs2GeomApply(&work, &source, &attribute, 0) == SCE_NGS2_OK);
    Ngs2GeomAttribute defaultAttribute;
    Require(sceNgs2GeomApply(&work, &source, &defaultAttribute, 15) == SCE_NGS2_OK);
    Require(std::memcmp(&attribute, &defaultAttribute, sizeof(attribute)) == 0);
    Require(Near(attribute.pitch_ratio, 1.1111111f));
    Require(attribute.a3d_attrib.position.z == 10.0f);
    Require(attribute.level[0] > 0.0f);

    Require(sceNgs2GeomApply(&work, &source, &attribute, 2) == SCE_NGS2_OK);
    Require(Near(attribute.pitch_ratio, 1.1111111f));
    Require(attribute.a3d_attrib.position.z == 0.0f);
    RequireZero(attribute, 0, 64);

    Require(sceNgs2GeomApply(&work, &source, &attribute, 4) == SCE_NGS2_OK);
    Require(attribute.pitch_ratio == 0.0f);
    Require(attribute.a3d_attrib.position.z == 0.0f);
    Require(attribute.level[0] > 0.0f);

    Require(sceNgs2GeomApply(&work, &source, &attribute, 8) == SCE_NGS2_OK);
    Require(attribute.pitch_ratio == 0.0f);
    Require(attribute.a3d_attrib.position.z == 10.0f);
    RequireZero(attribute, 0, 64);

    Require(sceNgs2GeomApply(&work, &source, &attribute, 1) == SCE_NGS2_OK);
    Require(attribute.pitch_ratio == 0.0f);
    Require(attribute.a3d_attrib.position.z == 10.0f);
    Require(attribute.level[0] > 0.0f);

    Require(sceNgs2GeomApply(&work, &source, &attribute, 16) == SCE_NGS2_OK);
    Require(attribute.pitch_ratio == 0.0f);
    Require(attribute.a3d_attrib.position.z == 0.0f);
    RequireZero(attribute, 0, 64);
}

int main() {
    TestLayouts();
    TestResetListenerParam();
    TestResetSourceParam();
    TestListenerTransform();
    TestInvalidListener();
    TestApplyStereo();
    TestApplySurround();
    TestApplyDistanceConeDoppler();
    TestApplyFlags();
    TestInvalidApply();
    return 0;
}
