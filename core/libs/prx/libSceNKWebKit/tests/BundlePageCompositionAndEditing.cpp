#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "WKBundlePage composition/editing check failed at line %u\n", location.line());
        std::abort();
    }
}

template<class Function>
void RequireInvalidArgument(Function function) {
    bool threw = false;
    try {
        function();
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
}

}

int main() {
    auto page = NKWebKit::Make<NKWebKit::BundlePage>();
    const void* pageRef = page.Get();

    Require(!WKBundlePageHasComposition(pageRef));
    Require(page->newlineInQuotedContentCount == 0);
    Require(page->pendingEditorStateUpdateFlushCount == 0);

    auto compText = NKWebKit::Make<NKWebKit::String>(std::u16string(u"composition"));
    auto highlightData = NKWebKit::Make<NKWebKit::Array>();
    auto annotationData = NKWebKit::Make<NKWebKit::Array>();

    WKBundlePageSetComposition(pageRef, compText.Get(), 2, 8, true, highlightData.Get(), annotationData.Get());
    Require(WKBundlePageHasComposition(pageRef));
    Require(page->hasComposition);
    Require(page->compositionText.Get() == compText.Get());
    Require(page->compositionFrom == 2);
    Require(page->compositionLength == 8);
    Require(page->suppressCompositionUnderline);
    Require(page->compositionHighlightData.Get() == highlightData.Get());
    Require(page->compositionAnnotationData.Get() == annotationData.Get());

    WKBundlePageConfirmComposition(pageRef);
    Require(!WKBundlePageHasComposition(pageRef));
    Require(!page->hasComposition);
    Require(page->compositionText.Get() == nullptr);

    WKBundlePageSetComposition(pageRef, compText.Get(), 0, 5, false, nullptr, nullptr);
    Require(WKBundlePageHasComposition(pageRef));
    Require(page->hasComposition);
    Require(!page->suppressCompositionUnderline);
    Require(page->compositionHighlightData.Get() == nullptr);
    Require(page->compositionAnnotationData.Get() == nullptr);

    auto confirmedText = NKWebKit::Make<NKWebKit::String>(std::u16string(u"confirmed"));
    WKBundlePageConfirmCompositionWithText(pageRef, confirmedText.Get());
    Require(!WKBundlePageHasComposition(pageRef));
    Require(!page->hasComposition);
    Require(page->compositionText.Get() == confirmedText.Get());

    WKBundlePageInsertNewlineInQuotedContent(pageRef);
    Require(page->newlineInQuotedContentCount == 1);
    WKBundlePageInsertNewlineInQuotedContent(pageRef);
    Require(page->newlineInQuotedContentCount == 2);

    WKBundlePageFlushPendingEditorStateUpdate(pageRef);
    Require(page->pendingEditorStateUpdateFlushCount == 1);
    WKBundlePageFlushPendingEditorStateUpdate(pageRef);
    Require(page->pendingEditorStateUpdateFlushCount == 2);

    auto matchIndices = NKWebKit::Make<NKWebKit::Array>();
    auto replacementText = NKWebKit::Make<NKWebKit::String>(std::u16string(u"replacement"));
    WKBundlePageReplaceStringMatches(pageRef, matchIndices.Get(), replacementText.Get(), true);
    Require(page->stringMatchesReplacementIndices.Get() == matchIndices.Get());
    Require(page->stringMatchesReplacementText.Get() == replacementText.Get());
    Require(page->stringMatchesSelectionOnly);

    WKBundlePageReplaceStringMatches(pageRef, matchIndices.Get(), replacementText.Get(), false);
    Require(!page->stringMatchesSelectionOnly);

    auto frame = NKWebKit::Make<NKWebKit::BundleFrame>();

    RequireInvalidArgument([&] { WKBundlePageConfirmComposition(nullptr); });
    RequireInvalidArgument([&] { WKBundlePageConfirmComposition(frame.Get()); });

    RequireInvalidArgument([&] { WKBundlePageConfirmCompositionWithText(nullptr, confirmedText.Get()); });
    RequireInvalidArgument([&] { WKBundlePageConfirmCompositionWithText(frame.Get(), confirmedText.Get()); });
    RequireInvalidArgument([&] { WKBundlePageConfirmCompositionWithText(pageRef, nullptr); });
    RequireInvalidArgument([&] { WKBundlePageConfirmCompositionWithText(pageRef, frame.Get()); });

    RequireInvalidArgument([&] { WKBundlePageSetComposition(nullptr, compText.Get(), 0, 0, false, nullptr, nullptr); });
    RequireInvalidArgument([&] { WKBundlePageSetComposition(frame.Get(), compText.Get(), 0, 0, false, nullptr, nullptr); });
    RequireInvalidArgument([&] { WKBundlePageSetComposition(pageRef, nullptr, 0, 0, false, nullptr, nullptr); });
    RequireInvalidArgument([&] { WKBundlePageSetComposition(pageRef, frame.Get(), 0, 0, false, nullptr, nullptr); });
    RequireInvalidArgument([&] { WKBundlePageSetComposition(pageRef, compText.Get(), 0, 0, false, frame.Get(), nullptr); });
    RequireInvalidArgument([&] { WKBundlePageSetComposition(pageRef, compText.Get(), 0, 0, false, nullptr, frame.Get()); });

    RequireInvalidArgument([&] { WKBundlePageInsertNewlineInQuotedContent(nullptr); });
    RequireInvalidArgument([&] { WKBundlePageInsertNewlineInQuotedContent(frame.Get()); });

    RequireInvalidArgument([&] { WKBundlePageFlushPendingEditorStateUpdate(nullptr); });
    RequireInvalidArgument([&] { WKBundlePageFlushPendingEditorStateUpdate(frame.Get()); });

    RequireInvalidArgument([&] { WKBundlePageReplaceStringMatches(nullptr, matchIndices.Get(), replacementText.Get(), true); });
    RequireInvalidArgument([&] { WKBundlePageReplaceStringMatches(frame.Get(), matchIndices.Get(), replacementText.Get(), true); });
    RequireInvalidArgument([&] { WKBundlePageReplaceStringMatches(pageRef, nullptr, replacementText.Get(), true); });
    RequireInvalidArgument([&] { WKBundlePageReplaceStringMatches(pageRef, frame.Get(), replacementText.Get(), true); });
    RequireInvalidArgument([&] { WKBundlePageReplaceStringMatches(pageRef, matchIndices.Get(), nullptr, true); });
    RequireInvalidArgument([&] { WKBundlePageReplaceStringMatches(pageRef, matchIndices.Get(), frame.Get(), true); });

    return 0;
}
