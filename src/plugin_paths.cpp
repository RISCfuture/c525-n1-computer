#include "plugin_paths.h"

#include "XPLMPlugin.h"
#include "XPLMUtilities.h"

namespace sfn1 {
namespace {

/// Both separators, because XPLM_USE_NATIVE_PATHS reports Windows paths with
/// backslashes and every other platform with forward slashes.
constexpr const char* kSeparators = "/\\";

}  // namespace

std::string pluginDir() {
    char xplPath[1024] = {};
    XPLMGetPluginInfo(XPLMGetMyID(), nullptr, xplPath, nullptr, nullptr);
    std::string path(xplPath);
    for (int i = 0; i < 2; ++i) {
        const auto separator = path.find_last_of(kSeparators);
        if (separator == std::string::npos) return ".";
        path.erase(separator);
    }
    return path;
}

}  // namespace sfn1
