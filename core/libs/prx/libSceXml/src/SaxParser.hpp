#pragma once

#include "prx/libSceXml/include/Api.hpp"

namespace XmlSax {

enum class ParserOption { SkipIgnorableWhiteSpace, ResolveEntity };

int ParserInitialize(XmlHandle* self, const XmlHandle* initializer);
int ParserTerminate(XmlHandle* self);
void ParserDestroy(XmlHandle* self);
int ParserReset(XmlHandle* self);
int ParserSetDocumentHandler(XmlHandle* self, void* handler);
void ParserSetUserData(XmlHandle* self, void* userData);
void ParserSetOption(XmlHandle* self, ParserOption option, bool value);
int ParserParse(XmlHandle* self, const XmlString* text, bool last);

}
