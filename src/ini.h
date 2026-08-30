#pragma once

#include <map>
#include <string>

namespace sfn1 {

/// Reads a flat `key=value` file of integers. Section headers and anything
/// that does not parse are skipped, so a malformed line costs that one setting
/// its value and nothing else. A missing file reads as empty.
std::map<std::string, int> readIni(const std::string& path);

/// The named setting, or fallback when it is absent.
int iniValue(const std::map<std::string, int>& ini, const std::string& key, int fallback);

}  // namespace sfn1
