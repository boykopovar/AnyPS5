#include "prx/libSceNKWebKit/src/Configuration.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"

#include <vector>

extern "C" {

const void* APS5_VABI WKPageCopyRelatedPages(const void* pageRef) {
    const auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    std::vector<NKWebKit::Retained<NKWebKit::Object>> elements;
    if (!page->isClosed && !page->isTerminated) {
        for (const auto& related : page->relatedPages) {
            if (related.Get() != page) {
                if (related->Type() == NKWebKit::ObjectType::Page) {
                    const auto* relatedPage = static_cast<const NKWebKit::Page*>(related.Get());
                    if (relatedPage->isClosed || relatedPage->isTerminated) {
                        continue;
                    }
                }
                elements.push_back(related);
            }
        }
    }
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::Array>(std::move(elements)));
}

std::int32_t APS5_VABI WKPageGetProcessIdentifier(const void* pageRef) {
    const auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    if (page->isClosed || page->isTerminated) {
        return 0;
    }
    return page->processIdentifier;
}

void APS5_VABI WKPageListenForLayoutMilestones(const void* pageRef, std::uint32_t milestones) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    if (page->isClosed || page->isTerminated) {
        return;
    }
    page->layoutMilestones = milestones;
}

void APS5_VABI WKPageSetMockCameraOrientation(const void* pageRef, std::uint32_t orientation) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    if (page->isClosed || page->isTerminated) {
        return;
    }
    page->mockCameraOrientation = orientation;
}

void APS5_VABI WKPageTerminate(const void* pageRef) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    if (page->isClosed) {
        return;
    }
    page->isTerminated = true;
}

void APS5_VABI WKPageUpdateWebsitePolicies(const void* pageRef, const void* websitePoliciesRef) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    const auto* policies = NKWebKit::Cast<NKWebKit::WebsitePolicies>(websitePoliciesRef, __func__);
    if (page->isClosed || page->isTerminated) {
        return;
    }
    page->websitePolicies = NKWebKit::Retained<NKWebKit::Object>::Share(const_cast<NKWebKit::WebsitePolicies*>(policies));
}

}
