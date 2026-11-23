#include "prx/libSceIcu/Normalizer2Accessors.hpp"
#include "prx/libc/include/General.hpp"
#include "norm2allmodes.h"
#include "normalizer2impl.h"
#include "unicode/normalizer2.h"
#include "unicode/stringpiece.h"
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace {

class GuestStatus {
public:
    GuestStatus(std::int32_t* error, const char* caller) : guest(error), name(caller) {
        if (!guest) throw std::invalid_argument(name + ": null status");
        std::int32_t value = 0;
        if (!GuestCopyIn_nid_no_patch(&value, guest, sizeof(value))) throw std::invalid_argument(name + ": unreadable status");
        initial = value;
        status = static_cast<UErrorCode>(value);
    }

    UErrorCode& Native() {
        return status;
    }

    void Store() const {
        const auto value = static_cast<std::int32_t>(status);
        if (value == initial) return;
        if (!GuestCopyOut_nid_no_patch(guest, &value, sizeof(value))) throw std::invalid_argument(name + ": unwritable status");
    }

private:
    std::int32_t* guest;
    std::string name;
    std::int32_t initial = 0;
    UErrorCode status = U_ZERO_ERROR;
};

const icu::Normalizer2* Native(const Icu::Normalizer2* normalizer, const char* caller) {
    if (!normalizer) throw std::invalid_argument(std::string(caller) + ": null normalizer");
    return reinterpret_cast<const icu::Normalizer2*>(normalizer);
}

const icu::Normalizer2Impl* Native(const Icu::Normalizer2Impl* impl, const char* caller) {
    if (!impl) throw std::invalid_argument(std::string(caller) + ": null normalizer data");
    return reinterpret_cast<const icu::Normalizer2Impl*>(impl);
}

const Icu::Normalizer2* Guest(const icu::Normalizer2* normalizer) {
    return reinterpret_cast<const Icu::Normalizer2*>(normalizer);
}

const Icu::Normalizer2Impl* Guest(const icu::Normalizer2Impl* impl) {
    return reinterpret_cast<const Icu::Normalizer2Impl*>(impl);
}

template <typename TResult, typename TCall>
TResult WithStatus(std::int32_t* error, const char* caller, TCall call) {
    GuestStatus status(error, caller);
    const TResult result = call(status.Native());
    status.Store();
    return result;
}

template <std::size_t Capacity, typename TLookup>
const char16_t* Decomposition(const Icu::Normalizer2Impl* impl, std::int32_t character, char16_t* buffer, std::int32_t* length,
                              const char* caller, TLookup lookup) {
    const auto* native = Native(impl, caller);
    if (!length) throw std::invalid_argument(std::string(caller) + ": null length");
    char16_t local[Capacity] = {};
    std::int32_t count = 0;
    const char16_t* result = lookup(*native, character, local, count);
    if (!result) return nullptr;
    if (result == local) {
        if (!buffer) throw std::invalid_argument(std::string(caller) + ": null buffer");
        if (count < 0 || static_cast<std::size_t>(count) > Capacity)
            throw std::logic_error(std::string(caller) + ": decomposition longer than its buffer");
        if (!GuestCopyOut_nid_no_patch(buffer, local, sizeof(char16_t) * static_cast<std::size_t>(count)))
            throw std::invalid_argument(std::string(caller) + ": unwritable buffer");
        result = buffer;
    }
    if (!GuestCopyOut_nid_no_patch(length, &count, sizeof(count))) throw std::invalid_argument(std::string(caller) + ": unwritable length");
    return result;
}

}

extern "C" {

const Icu::Normalizer2* APS5_VABI _ZN6icu_6811Normalizer214getNFCInstanceER10UErrorCode_nid_postfix(std::int32_t* error) {
    return WithStatus<const Icu::Normalizer2*>(error, __func__, [](UErrorCode& status) {
        return Guest(icu::Normalizer2::getNFCInstance(status));
    });
}

const Icu::Normalizer2* APS5_VABI _ZN6icu_6811Normalizer214getNFDInstanceER10UErrorCode_nid_postfix(std::int32_t* error) {
    return WithStatus<const Icu::Normalizer2*>(error, __func__, [](UErrorCode& status) {
        return Guest(icu::Normalizer2::getNFDInstance(status));
    });
}

const Icu::Normalizer2* APS5_VABI _ZN6icu_6811Normalizer223getNFKCCasefoldInstanceER10UErrorCode_nid_postfix(std::int32_t* error) {
    return WithStatus<const Icu::Normalizer2*>(error, __func__, [](UErrorCode& status) {
        return Guest(icu::Normalizer2::getNFKCCasefoldInstance(status));
    });
}

const Icu::Normalizer2* APS5_VABI _ZN6icu_6818Normalizer2Factory11getInstanceE18UNormalizationModeR10UErrorCode_nid_postfix(
    std::int32_t mode, std::int32_t* error) {
    return WithStatus<const Icu::Normalizer2*>(error, __func__, [mode](UErrorCode& status) {
        return Guest(icu::Normalizer2Factory::getInstance(static_cast<UNormalizationMode>(mode), status));
    });
}

const Icu::Normalizer2* APS5_VABI _ZN6icu_6818Normalizer2Factory15getNoopInstanceER10UErrorCode_nid_postfix(std::int32_t* error) {
    return WithStatus<const Icu::Normalizer2*>(error, __func__, [](UErrorCode& status) {
        return Guest(icu::Normalizer2Factory::getNoopInstance(status));
    });
}

const Icu::Normalizer2Impl* APS5_VABI _ZN6icu_6818Normalizer2Factory11getNFKCImplER10UErrorCode_nid_postfix(std::int32_t* error) {
    return WithStatus<const Icu::Normalizer2Impl*>(error, __func__, [](UErrorCode& status) {
        return Guest(icu::Normalizer2Factory::getNFKCImpl(status));
    });
}

const Icu::Normalizer2Impl* APS5_VABI _ZN6icu_6818Normalizer2Factory14getNFKC_CFImplER10UErrorCode_nid_postfix(std::int32_t* error) {
    return WithStatus<const Icu::Normalizer2Impl*>(error, __func__, [](UErrorCode& status) {
        return Guest(icu::Normalizer2Factory::getNFKC_CFImpl(status));
    });
}

const Icu::Normalizer2Impl* APS5_VABI _ZN6icu_6818Normalizer2Factory7getImplEPKNS_11Normalizer2E_nid_postfix(
    const Icu::Normalizer2* normalizer) {
    const auto* withImpl = dynamic_cast<const icu::Normalizer2WithImpl*>(Native(normalizer, __func__));
    if (!withImpl) NotImplemented_nid_no_patch("Normalizer2Factory::getImpl: normalizer without normalization data");
    return Guest(icu::Normalizer2Factory::getImpl(withImpl));
}

std::int32_t APS5_VABI _ZNK6icu_6811Normalizer211composePairEii_nid_postfix(const Icu::Normalizer2* normalizer, std::int32_t first,
                                                                           std::int32_t second) {
    return Native(normalizer, __func__)->icu::Normalizer2::composePair(first, second);
}

std::uint8_t APS5_VABI _ZNK6icu_6811Normalizer217getCombiningClassEi_nid_postfix(const Icu::Normalizer2* normalizer,
                                                                                std::int32_t character) {
    return Native(normalizer, __func__)->icu::Normalizer2::getCombiningClass(character);
}

std::int8_t APS5_VABI _ZNK6icu_6811Normalizer216isNormalizedUTF8ENS_11StringPieceER10UErrorCode_nid_postfix(
    const Icu::Normalizer2* normalizer, const char* data, std::int32_t length, std::int32_t* error) {
    const auto* native = Native(normalizer, __func__);
    return WithStatus<std::int8_t>(error, __func__, [native, data, length](UErrorCode& status) {
        return static_cast<std::int8_t>(native->icu::Normalizer2::isNormalizedUTF8(icu::StringPiece(data, length), status));
    });
}

std::int32_t APS5_VABI _ZNK6icu_6815Normalizer2Impl11composePairEii_nid_postfix(const Icu::Normalizer2Impl* impl, std::int32_t first,
                                                                               std::int32_t second) {
    return Native(impl, __func__)->composePair(first, second);
}

const char16_t* APS5_VABI _ZNK6icu_6815Normalizer2Impl16getDecompositionEiPDsRi_nid_postfix(const Icu::Normalizer2Impl* impl,
                                                                                          std::int32_t character, char16_t* buffer,
                                                                                          std::int32_t* length) {
    return Decomposition<4>(impl, character, buffer, length, __func__,
                         [](const icu::Normalizer2Impl& native, std::int32_t c, char16_t* local, std::int32_t& count) {
                             return native.getDecomposition(c, local, count);
                         });
}

const char16_t* APS5_VABI _ZNK6icu_6815Normalizer2Impl19getRawDecompositionEiPDsRi_nid_postfix(const Icu::Normalizer2Impl* impl,
                                                                                             std::int32_t character,
                                                                                             char16_t* buffer,
                                                                                             std::int32_t* length) {
    return Decomposition<30>(impl, character, buffer, length, __func__,
                         [](const icu::Normalizer2Impl& native, std::int32_t c, char16_t* local, std::int32_t& count) {
                             return native.getRawDecomposition(c, local, count);
                         });
}

std::int8_t APS5_VABI _ZNK6icu_6815Normalizer2Impl23hasDecompBoundaryBeforeEi_nid_postfix(const Icu::Normalizer2Impl* impl,
                                                                                         std::int32_t character) {
    return static_cast<std::int8_t>(Native(impl, __func__)->hasDecompBoundaryBefore(character));
}

std::int8_t APS5_VABI _ZNK6icu_6815Normalizer2Impl29norm16HasDecompBoundaryBeforeEt_nid_postfix(const Icu::Normalizer2Impl* impl,
                                                                                              std::uint16_t norm16) {
    return static_cast<std::int8_t>(Native(impl, __func__)->norm16HasDecompBoundaryBefore(norm16));
}

std::int8_t APS5_VABI _ZNK6icu_6815Normalizer2Impl19ensureCanonIterDataER10UErrorCode_nid_postfix(const Icu::Normalizer2Impl* impl,
                                                                                                 std::int32_t* error) {
    const auto* native = Native(impl, __func__);
    return WithStatus<std::int8_t>(error, __func__, [native](UErrorCode& status) {
        return static_cast<std::int8_t>(native->ensureCanonIterData(status));
    });
}

}
