#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"

extern "C" {

void APS5_VABI WKBundlePageConfirmComposition(const void* pageRef) {
    auto page = NKWebKit::Cast<NKWebKit::BundlePage>(pageRef, __func__);
    page->hasComposition = false;
    page->compositionText = nullptr;
}

void APS5_VABI WKBundlePageConfirmCompositionWithText(const void* pageRef, const void* textRef) {
    auto page = NKWebKit::Cast<NKWebKit::BundlePage>(pageRef, __func__);
    auto text = NKWebKit::Cast<NKWebKit::String>(textRef, __func__);
    page->hasComposition = false;
    page->compositionText = NKWebKit::Retained<NKWebKit::String>::Share(text);
}

void APS5_VABI WKBundlePageSetComposition(const void* pageRef, const void* textRef, int from, int length, bool suppressUnderline, const void* highlightDataRef, const void* annotationDataRef) {
    auto page = NKWebKit::Cast<NKWebKit::BundlePage>(pageRef, __func__);
    auto text = NKWebKit::Cast<NKWebKit::String>(textRef, __func__);
    auto highlightData = NKWebKit::CastOrNull<NKWebKit::Array>(highlightDataRef, __func__);
    auto annotationData = NKWebKit::CastOrNull<NKWebKit::Array>(annotationDataRef, __func__);
    page->hasComposition = true;
    page->compositionText = NKWebKit::Retained<NKWebKit::String>::Share(text);
    page->compositionFrom = from;
    page->compositionLength = length;
    page->suppressCompositionUnderline = suppressUnderline;
    page->compositionHighlightData = NKWebKit::Retained<NKWebKit::Array>::Share(highlightData);
    page->compositionAnnotationData = NKWebKit::Retained<NKWebKit::Array>::Share(annotationData);
}

void APS5_VABI WKBundlePageInsertNewlineInQuotedContent(const void* pageRef) {
    auto page = NKWebKit::Cast<NKWebKit::BundlePage>(pageRef, __func__);
    page->newlineInQuotedContentCount++;
}

void APS5_VABI WKBundlePageFlushPendingEditorStateUpdate(const void* pageRef) {
    auto page = NKWebKit::Cast<NKWebKit::BundlePage>(pageRef, __func__);
    page->pendingEditorStateUpdateFlushCount++;
}

void APS5_VABI WKBundlePageReplaceStringMatches(const void* pageRef, const void* matchIndicesRef, const void* replacementTextRef, bool selectionOnly) {
    auto page = NKWebKit::Cast<NKWebKit::BundlePage>(pageRef, __func__);
    auto matchIndices = NKWebKit::Cast<NKWebKit::Array>(matchIndicesRef, __func__);
    auto replacementText = NKWebKit::Cast<NKWebKit::String>(replacementTextRef, __func__);
    page->stringMatchesReplacementIndices = NKWebKit::Retained<NKWebKit::Array>::Share(matchIndices);
    page->stringMatchesReplacementText = NKWebKit::Retained<NKWebKit::String>::Share(replacementText);
    page->stringMatchesSelectionOnly = selectionOnly;
}

}
