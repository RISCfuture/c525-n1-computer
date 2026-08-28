# Vendored dependencies

Everything this plugin links is committed here in source form rather than
fetched at build time. `scripts/update-vendor.sh` refreshes these from their
pinned upstream versions; the pins live at the top of that script and are the
single place a version is recorded.

```sh
scripts/update-vendor.sh --check   # report drift, change nothing
scripts/update-vendor.sh           # refresh in place, then review and commit
```

## Why vendored, and not fetched at build time

- **The X-Plane SDK is not a git repository.** It is a zip published on
  Laminar's website from an undocumented path that has changed between
  releases. Fetching it during a build makes every build, on every branch,
  depend on that URL still resolving.
- **Old tags have to keep building.** `git checkout v0.1.0 && cmake` reproduces
  the binary that shipped, offline, with no setup step. That is worth more here
  than the ~11 MB the vendored trees cost in a 35 MB clone.
- **Two of these are single headers.** Fetch machinery for `stb_image.h` and
  `doctest.h` is larger than the thing it manages.
- **ImgWindow cannot be fetched at all** — see below.

The update script exists so that vendoring does not also mean *stale*: the pins
are explicit, and `--check` says whether the tree still matches them.

## What is here

| Component | Version | Upstream | Licence |
| --- | --- | --- | --- |
| `imgui/` | 1.92.8 | <https://github.com/ocornut/imgui> | MIT (`imgui/LICENSE.txt`) |
| `stb/stb_image.h` | v2.19 (commit `fa2a1d9b`) | <https://github.com/nothings/stb> | MIT / public domain (`stb/LICENSE`) |
| `doctest/doctest.h` | 2.5.3 | <https://github.com/doctest/doctest> | MIT (`doctest/LICENSE.txt`) |
| `ImgWindow/` | derivative — see below | <https://github.com/xsquawkbox/xsb_public> | BSD-3-Clause (`ImgWindow/COPYING.md`) |
| `../SDK/` | 4.3.0 | <https://developer.x-plane.com/sdk/> | BSD-style (`../SDK/license.txt`) |

The SDK sits at the repository root rather than here because that is the layout
Laminar ships and every X-Plane build script expects.

stb publishes no tags, so it is pinned by commit. That commit predates stb
having a `LICENSE` file, so `stb/LICENSE` is taken from the current upstream
tree instead; its terms are unchanged.

## ImgWindow is project source, not a tracked dependency

`ImgWindow/` began as Christopher Collins' ImgWindow from `xsb_public`, but it
is no longer a copy of it, and **it must not be re-fetched**:

- Upstream's last commit is from **July 2020** and targets a much older Dear
  ImGui. Our copy carries the adaptation to ImGui 1.90+ — 41 uses of
  `IMGUI_V190_REFACTOR` and the modern texture-ID API that upstream has none of.
- `SystemGL.h` is not shipped upstream at all; ImgWindow `#include`s it and
  expects the consumer to provide it. Ours additionally handles Windows, which
  ships the OpenGL 1.1 headers and no `glext.h`: the Windows branch includes
  `<GL/gl.h>` and defines `GL_CLAMP_TO_EDGE`, the only symbol past 1.1 this
  codebase uses.
- The window gestures are ours. `dragTargetAt`, `constrainDrag` and `cursorAt`
  let a subclass place its own move, resize and cursor zones — which is how the
  faceplate gets corner gestures and a fixed aspect ratio — and right-clicks no
  longer drag the window.

Overwriting this directory from upstream would break the build. Treat it as our
own source that happens to carry an upstream copyright notice, and keep the
BSD-3-Clause attribution with it. `scripts/update-vendor.sh` deliberately leaves
it alone.

## Updating

Bump a pin in `scripts/update-vendor.sh`, run it, then build all three platforms
and run `tests/run_tests.sh` before committing. ImGui is the one to be careful
with: our ImgWindow derivative tracks its API, so a major ImGui bump usually
needs matching changes here in the same commit.
