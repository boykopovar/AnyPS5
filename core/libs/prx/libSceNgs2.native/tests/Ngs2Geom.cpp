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
    Ngs2GeomListenerWork work;
    std::memset(&work, 0x5a, sizeof(work));
    unsigned char before[sizeof(work)];
    std::memcpy(before, &work, sizeof(work));
    Require(sceNgs2GeomCalcListener(nullptr, &work, 0) == static_cast<int>(0x804A8461));
    Require(sceNgs2GeomCalcListener(nullptr, &work, 2) == static_cast<int>(0x804A8461));
    Require(sceNgs2GeomCalcListener(nullptr, nullptr, 0) == static_cast<int>(0x804A8461));
    Require(sceNgs2GeomCalcListener(&param, &work, 2) == static_cast<int>(0x804A8463));
    Require(sceNgs2GeomCalcListener(&param, &work, 0x80000000u) == static_cast<int>(0x804A8463));
    Require(sceNgs2GeomCalcListener(&param, &work, 0xffffffffu) == static_cast<int>(0x804A8463));
    Require(sceNgs2GeomCalcListener(&param, nullptr, 2) == static_cast<int>(0x804A8010));
    Require(std::memcmp(before, &work, sizeof(work)) == 0);
    for (const float value : {0.0f, -1.0f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()}) {
        param.sound_speed = value;
        RejectListener(&param, 0);
    }
    param = Listener();
    param.orient_front = {};
    RejectListener(&param, 0);
    param = Listener();
    param.orient_up = {};
    RejectListener(&param, 0);
    param = Listener();
    param.orient_up = param.orient_front;
    Ngs2GeomListenerWork parallelWork{};
    Require(sceNgs2GeomCalcListener(&param, &parallelWork, 0) == SCE_NGS2_OK);
    RequireMatrix(parallelWork, {{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}});
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

static void TestGeomApply() {
    Ngs2GeomListenerWork listener{};
    for (std::size_t row = 0; row < 4; row++) {
        for (std::size_t column = 0; column < 4; column++) listener.matrix[row][column] = row == column ? 1.0f : 0.0f;
    }

    Ngs2GeomSourceParam source;
    Require(sceNgs2GeomResetSourceParam(&source) == SCE_NGS2_OK);
    source.position = {1.0f, 2.0f, 3.0f};
    source.min_level = -0.25f;
    source.max_level = 0.5f;
    source.matrix_format = 3;

    Ngs2GeomAttribute attribute;
    std::memset(&attribute, 0x5a, sizeof(attribute));
    Require(sceNgs2GeomApply(&listener, &source, &attribute, 0) == SCE_NGS2_OK);
    Require(attribute.pitch_ratio == 1.0f);
    Require(attribute.a3d_attrib.position.x == 1.0f && attribute.a3d_attrib.position.y == 2.0f && attribute.a3d_attrib.position.z == 3.0f);
    Require(attribute.a3d_attrib.volume == 0.5f);
    Require(attribute.level[0] == 0.5f);
    Require(attribute.level[1] == 0.0f);
    Require(attribute.level[9] == 0.5f);
    Require(attribute.level[18] == 0.5f);
    Require(attribute.level[24] == 0.0f);
    Require(attribute.level[63] == 0.0f);

    Require(sceNgs2GeomApply(&listener, &source, nullptr, 0) == SCE_NGS2_ERROR_INVALID_OUT_ADDRESS);

    source.matrix_format = 0;
    std::memset(&attribute, 0x5a, sizeof(attribute));
    Require(sceNgs2GeomApply(&listener, &source, &attribute, 0) == SCE_NGS2_OK);
    Require(attribute.level[0] == 0.5f);
    Require(attribute.level[9] == 0.5f);
    Require(attribute.level[16] == 0.0f);
}

int main() {
    TestLayouts();
    TestResetListenerParam();
    TestResetSourceParam();
    TestListenerTransform();
    TestInvalidListener();
    TestGeomApply();
    return 0;
}
