#pragma once

#include <string>

namespace AsanRuntime::ReportFile {

void SetPath_nid_no_patch(const char* path);
void SetDescriptor_nid_no_patch(int descriptor);
const char* GetPath_nid_no_patch();
void Write_nid_no_patch(const std::string& text);

}  // namespace AsanRuntime::ReportFile
