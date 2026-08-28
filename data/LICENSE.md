# Terms for `data/`

**These files are not covered by the MIT licence in the repository root.**

The eight CSV files here are N1 thrust schedules transcribed by hand, chart cell
by chart cell, from the *TorqueSim Cessna Citation 525 Performance Manual*. The
transcription is documented in [`PROVENANCE.md`](PROVENANCE.md).

## Where the numbers came from

That performance manual is **community work, not a TorqueSim, X-Aviation or
Textron document**. It was built and published free by **hornetaircraft**, with
**Aviationsocal**, **Kaboom**, **Jetpipeoverheat** and **Cptlee** assisting, and
it lives on the X-Pilot forums:

<https://forums.x-pilot.com/files/file/1613-torquesim-cessna-citation-525-performance-manual/>

Its own description states the charts are modelled as closely as possible on the
real Textron charts. Nothing here was extracted from the payware aircraft, from
its files, or from any document that ships with it.

The transcription was additionally cross-checked cell by cell against the data
tables in **Matchstick's** [CJ525 N1
Calculator](https://forums.x-pilot.com/files/file/1635-cj525-n1-calculator/),
the FlyWithLua script that first brought these schedules into X-Plane. Where the
two disagree, `PROVENANCE.md` records which reading the source pages support.

## What this repository claims

Nothing. The underlying performance figures are not this author's work and no
ownership of them is claimed — only the CSV encoding, the adjudication of the
source manual's internal contradictions, and the plugin that reads them.

They are included for one purpose: so the Safe Flight N1 Computer plugin can
reproduce the instrument's behaviour for owners of the aircraft the data
describes.

If you want these schedules for your own project, please take them from
hornetaircraft's manual or Matchstick's calculator and credit their authors,
rather than lifting the CSVs from here — the credit belongs upstream.

The values are approximate, community-transcribed, and **not for real world
use**. Do not treat them as an authoritative source.

If you hold rights in this data and would like it removed, please open an issue.
