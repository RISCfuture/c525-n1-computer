#include "ini.h"

#include <fstream>

namespace sfn1 {

std::map<std::string, int> readIni(const std::string& path) {
    std::map<std::string, int> values;
    std::ifstream file(path);
    std::string line;
    while (std::getline(file, line)) {
        const auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        try {
            values[line.substr(0, eq)] = std::stoi(line.substr(eq + 1));
        } catch (...) {
            // A malformed value means that setting falls back to its default.
        }
    }
    return values;
}

int iniValue(const std::map<std::string, int>& ini, const std::string& key, int fallback) {
    const auto it = ini.find(key);
    return it == ini.end() ? fallback : it->second;
}

}  // namespace sfn1
