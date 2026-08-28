# Safe Flight N1 Computer

A simulation of the Safe Flight N1 Computer/Display (P/N C-12732-1) for
X-Plane 12 and the TorqueSim CitationJet 525.

**Read `SafeFlightN1-Manual.pdf` in this folder** — it covers operation,
limitations, and troubleshooting. It is also a standalone download:

    https://github.com/RISCfuture/c525-n1-computer/releases/latest/download/SafeFlightN1-Manual.pdf

## Installing

Find your X-Plane 12 installation — the folder containing `X-Plane.exe`,
`X-Plane.app` or `X-Plane-x86_64`, wherever you installed it. Inside it, open
`Resources`, then `plugins`, and copy this entire `SafeFlightN1` folder in, so
you end up with:

    <your X-Plane 12 folder>/Resources/plugins/SafeFlightN1/

Keep the folder intact: the plugin loads its thrust tables from `data/` and its
faceplate art from `assets/` at runtime.

Start X-Plane and load the CitationJet 525. The instrument appears under
**Plugins → Safe Flight N1 Computer**. It stays inert with any other aircraft.

To remove it, delete the folder.

## Requirements

- X-Plane 12.3.0 or newer
- The TorqueSim CitationJet 525
- macOS 12+, Windows 10+, or Linux

## Staying up to date

Two ways to get new versions.

**SkunkCrafts Updater.** This plugin ships a SkunkCrafts manifest, so
[the updater](https://forums.x-plane.org/index.php?/forums/forum/311-skunkcrafts-updater/)
offers new versions automatically once this folder is in `plugins`.

**GitHub releases.** Every build is published here with notes on what changed:

    https://github.com/RISCfuture/c525-n1-computer/releases

## Credits

The N1 thrust schedules are not this plugin author's work. They were transcribed
from the *TorqueSim Cessna Citation 525 Performance Manual*, a community-built
chart set — not a TorqueSim, X-Aviation or Textron document — by
**hornetaircraft**, assisted by **Aviationsocal**, **Kaboom**,
**Jetpipeoverheat** and **Cptlee**:

    https://forums.x-pilot.com/files/file/1613-torquesim-cessna-citation-525-performance-manual/

Without their charts there would be nothing for this instrument to display.

## Licence and disclaimer

The plugin is MIT licensed; see `LICENSE`. The thrust tables in `data/` are
redistributed under separate terms; see `data/LICENSE.md`.

Not affiliated with, endorsed by, or supported by Safe Flight Instrument, LLC,
Textron Aviation, or TorqueSim. Performance data is community-sourced and
approximate. This is an advisory simulation — **not for real world use**.
