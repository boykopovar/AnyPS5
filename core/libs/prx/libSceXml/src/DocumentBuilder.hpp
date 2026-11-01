#pragma once

#include "prx/libSceXml/include/Api.hpp"

namespace XmlDom {

enum class BuilderOption { ResolveEntity, SkipIgnorableWhiteSpace, SkipIgnorableText };

int BuilderInitialize(XmlHandle* self, const XmlHandle* initializer);
int BuilderTerminate(XmlHandle* self);
void BuilderDestroy(XmlHandle* self);
int BuilderParse(XmlHandle* self, const XmlString* text, bool last);
void BuilderSetOption(XmlHandle* self, BuilderOption option, bool value);

}
