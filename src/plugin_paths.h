#pragma once

#include <string>

namespace sfn1 {

/// Absolute path to the plugin's own folder: the `SafeFlightN1` directory that
/// holds `data/`, `assets/` and the per-platform subfolder the `.xpl` sits in.
/// Returns "." if X-Plane reports a path with too few directory components.
std::string pluginDir();

}  // namespace sfn1
