# Changelog

Notable changes to the SafeFlightN1 plugin. Format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/); versions follow
[semantic versioning](https://semver.org/spec/v2.0.0.html).

## [0.2.0] — 2026-08-30

The device's own FAA-approved flight manual supplement (Cessna Model 525,
Section V, Supplement 6, _SafeFlight N1 Reminder_) and Cessna's Model 525
Operating Manual thrust-setting charts both turned up. Everything here follows
from reading them.

**The N1 schedules are unchanged** — all eight CSVs in `data/` are byte-for-byte
what 0.1.0 shipped. The Operating Manual charts were used to check them, not to
replace them; see below.

### Changed

- **The takeoff target now holds after liftoff.** Leave the knob alone through
  the climb-out and the display keeps the takeoff target it was showing on the
  ground — computed from the temperature you selected and the field elevation
  you left — until you move the knob. Go-around arrives when you reselect
  `TO/GA`, which is how the supplement introduces it.

  Previously the display switched to go-around at the live temperature and
  pressure altitude the instant the wheels came up, so a crew that dialled an
  assumed temperature lost their target and then watched the number drift
  through the climb. A selected temperature now lives as long as the hold and is
  released with it, at the first mode change or on touchdown, rather than being
  discarded at liftoff.

### Added

- **Hot-and-high trim for climb and cruise**, off by default. Set `isa_trim=1`
  in `config.ini` beside the `.xpl` to apply the Operating Manual's reduction:
  climb at or above 25,000 ft and cruise at or above 30,000 ft lose 1.0 %N1 at
  ISA+11…20 °C and 2.0 at ISA+21…30, on the anti-ice-off schedule only. The
  supplement does not say the real device does this, so the default keeps the
  supplement's behaviour.
- `config.ini`, for options that should outlive the window state. `settings.ini`
  is rewritten wholesale every time the window closes and would drop them.
- `sfn1/isa_trim`, a writable dataref mirroring the flag, so both behaviours can
  be compared without restarting.
- `scripts/digitize_om_charts.py`, which fits the Operating Manual's charts and
  compares them against `data/`. It needs your own copy of the charts; they are
  not redistributable and are not in the repository.

### Fixed

- **W/S BLEED AIR in its lower position read as off**, so an airplane with all
  three bleed air anti-ice systems selected could show dashes instead of the
  anti-ice schedule. Either live detent of that switch now counts. The
  `WING/ENGINE` switches still have to be fully up — their lower detent
  is engine inlet heat with the wings cold, which is not the full configuration.
- The manual documented the wrong switch for the anti-ice interlock, naming
  engine bleed where the device actually watches W/S BLEED AIR.

### Documentation

- Behaviour is now sourced from the flight manual supplement itself rather than
  from FlightSafety's training manual paraphrasing it, and the documentation
  notes the name the flight manual uses for the box.
- `data/PROVENANCE.md` records an independent check of the schedules against
  Cessna's Operating Manual charts. They agree to about half a percent N1 —
  roughly what reading a printed graph supports, and coarser than the tables'
  own tenth, so the charts corroborate rather than supersede. The check does
  settle the two blocks previously flagged as carrying known uncertainty: at
  every cell where this repo departed from a printed community value, the
  manufacturer's curve is closer to the value adopted than to the one rejected.
- `docs/CONTRACTS.md` records three schedules the charts publish that this
  device does not model: engine-only anti-ice, the environmental-bleed split,
  and (before this release) the hot-and-high trim.

## [0.1.0] — 2026-08-28

Initial release.

[0.2.0]: https://github.com/RISCfuture/c525-n1-computer/compare/v0.1.0...v0.2.0
[0.1.0]: https://github.com/RISCfuture/c525-n1-computer/releases/tag/v0.1.0
