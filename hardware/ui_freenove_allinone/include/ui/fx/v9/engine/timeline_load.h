#pragma once
#include "ui/fx/v9/engine/timeline.h"
#include "ui/fx/v9/engine/json_iface.h"
#include <string>

namespace fx {

// Load Timeline from JSON using a provided JSON parser adapter.
// For simplicity, this loader only supports numeric/string/bool param values and stores them as strings.
bool loadTimelineFromJson(Timeline& out, IJsonParser& parser, const std::string& text);

// Utility: parse a string param as number/bool with defaults
float paramFloat(const std::unordered_map<std::string,std::string>& m, const char* k, float def);
int   paramInt  (const std::unordered_map<std::string,std::string>& m, const char* k, int def);
bool  paramBool (const std::unordered_map<std::string,std::string>& m, const char* k, bool def);
const char* paramStr(const std::unordered_map<std::string,std::string>& m, const char* k, const char* def);

} // namespace fx
