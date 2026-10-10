#include "SceTypes.hpp"

#include <Testing/Test.hpp>

#include <stdexcept>

extern "C" int APS5_VABI sceContentExportInit2(const ContentExportInitParam2* init_param);
extern "C" int APS5_VABI sceContentExportTerm(void);

namespace {

using Testing::Case;
using Testing::RequireEqual;
using Testing::RequireThrows;

int allocator = 0;

ContentExportInitParam2 ValidParam() {
    return ContentExportInitParam2{&allocator, &allocator, nullptr, 0x10000, 0, 0};
}

class InitializedExport {
public:
    InitializedExport() {
        const auto param = ValidParam();
        RequireEqual(sceContentExportInit2(&param), 0, "initialize");
    }

    ~InitializedExport() {
        if (!initialized) return;
        try {
            sceContentExportTerm();
        } catch (const std::exception&) {
        }
    }

    InitializedExport(const InitializedExport&) = delete;
    InitializedExport& operator=(const InitializedExport&) = delete;

    int Terminate() {
        initialized = false;
        return sceContentExportTerm();
    }

private:
    bool initialized = true;
};

const Case nullParam{"Init2_NullParam_ThrowsLogicError", [] {
    RequireThrows<std::logic_error>([] { sceContentExportInit2(nullptr); }, "init with a null param");
}};

const Case termWithoutInit{"Term_NotInitialized_ThrowsLogicError", [] {
    RequireThrows<std::logic_error>([] { sceContentExportTerm(); }, "term without init");
}};

const Case missingFree{"Init2_MissingFreeFunction_ThrowsLogicError", [] {
    auto missing = ValidParam();
    missing.free_func = nullptr;
    RequireThrows<std::logic_error>([&] { sceContentExportInit2(&missing); }, "init without a free function");
}};

const Case nonZeroReserved{"Init2_NonZeroReservedField_ThrowsLogicError", [] {
    auto reserved = ValidParam();
    reserved.reserved1 = 1;
    RequireThrows<std::logic_error>([&] { sceContentExportInit2(&reserved); }, "init with a reserved field set");
}};

const Case initTwice{"Init2_AlreadyInitialized_ThrowsLogicError", [] {
    const InitializedExport exporter;
    const auto param = ValidParam();
    RequireThrows<std::logic_error>([&] { sceContentExportInit2(&param); }, "second init");
}};

const Case termTwice{"Term_AfterInit_SucceedsOnceThenThrows", [] {
    InitializedExport exporter;
    RequireEqual(exporter.Terminate(), 0, "first term");
    RequireThrows<std::logic_error>([] { sceContentExportTerm(); }, "second term");
}};

const Case reinitialize{"Init2_AfterTerm_InitializesAgain", [] {
    {
        InitializedExport first;
        RequireEqual(first.Terminate(), 0, "first term");
    }
    InitializedExport second;
    RequireEqual(second.Terminate(), 0, "term after reinitialization");
}};

} // namespace
