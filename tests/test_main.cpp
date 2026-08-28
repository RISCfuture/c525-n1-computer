#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cmath>
#include <cstdlib>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>

#include "../src/n1_computer.h"
#include "../src/n1_tables.h"

using sfn1::DisplayState;
using sfn1::InputSnapshot;
using sfn1::Mode;
using sfn1::N1Computer;
using sfn1::N1Table;
using sfn1::Output;

namespace {

// doctest::Approx is a relative comparison, which is the wrong shape for N1
// percentages checked to 1e-9, so tolerance stays absolute. The decomposition
// still reports the actual delta against the bound on failure.
#define CHECK_NEAR(actual, expected, tolerance) \
    CHECK(std::fabs((actual) - (expected)) <= (tolerance))

// The fixture and shipped-table directories come from the environment so that
// argv belongs entirely to doctest: -tc filters cases, --list-test-cases
// enumerates them. run_tests.sh sets both.
std::string envOr(const char* name, const char* fallback) {
    const char* value = std::getenv(name);
    return value != nullptr ? value : fallback;
}

std::string fixturesDirectory() { return envOr("SFN1_FIXTURES_DIR", "tests/fixtures"); }

std::string dataDirectory() { return envOr("SFN1_DATA_DIR", "data"); }

std::optional<std::string> readFile(const std::string& path) {
    std::ifstream file(path);
    if (!file) return std::nullopt;
    std::ostringstream text;
    text << file.rdbuf();
    return text.str();
}

N1Table loadFixtureTable(const std::string& path) {
    const auto text = readFile(path);
    REQUIRE_MESSAGE(text.has_value(), "fixture file unreadable: ", path);
    const auto table = N1Table::loadFromCsv(text.value_or(""));
    REQUIRE_MESSAGE(table.has_value(), "fixture table does not parse: ", path);
    return *table;
}

InputSnapshot onGroundAt(double ratC = 0.0, double paFt = 1000.0) {
    return {.powered = true,
            .onGround = true,
            .ratC = ratC,
            .pressureAltFt = paFt,
            .antiIce = 0,
            .knobPressed = false};
}

InputSnapshot airborneAt(double ratC = 0.0, double paFt = 1000.0) {
    return {.powered = true,
            .onGround = false,
            .ratC = ratC,
            .pressureAltFt = paFt,
            .antiIce = 0,
            .knobPressed = false};
}

Output run(N1Computer& computer, const InputSnapshot& input, double seconds,
           double step = 0.1) {
    Output out{};
    const long steps = std::lround(seconds / step);
    for (long i = 0; i < steps; ++i) out = computer.tick(input, step);
    return out;
}

N1Computer poweredOnGround(const std::string& fixturesDir) {
    N1Computer computer;
    REQUIRE(computer.loadTables(fixturesDir));
    run(computer, onGroundAt(), 5.2);
    return computer;
}

// --- N1Table ---------------------------------------------------------------

TEST_CASE("N1Table: bilinear interpolation and grid edges") {
    const std::string fixturesDir = fixturesDirectory();
    const N1Table takeoff = loadFixtureTable(fixturesDir + "/n1_takeoff.csv");

    CHECK_NEAR(takeoff.lookup(-5.0, 1000.0).value_or(-1), 91.5, 1e-9);  // interior bilinear
    CHECK_NEAR(takeoff.lookup(-5.0, 500.0).value_or(-1), 91.25, 1e-9);  // asymmetric weights
    CHECK_NEAR(takeoff.lookup(0.0, 2000.0).value_or(-1), 93.0, 1e-9);   // exact grid point
    // Outside the charted axes there is no data: the device dashes rather than
    // presenting an extrapolated thrust setting as a real one.
    CHECK(!takeoff.lookup(-40.0, 1000.0).has_value());  // below min OAT
    CHECK(!takeoff.lookup(-5.0, 99999.0).has_value());  // above max PA
    CHECK(!takeoff.lookup(100.0, -500.0).has_value());  // both axes outside
    CHECK(takeoff.lookup(-10.0, 0.0).has_value());      // exactly on the axis minimum
    // Negative pressure altitude is a high-QNH artifact, not an off-chart
    // condition: the bottom altitude line is the sea-level datum.
    CHECK_NEAR(takeoff.lookup(0.0, -50.0).value_or(-1), takeoff.lookup(0.0, 0.0).value_or(-2),
               1e-9);
    CHECK(takeoff.lookup(20.0, 4000.0).has_value());  // exactly on the axis maximum

    CHECK(!takeoff.lookup(5.0, 1000.0).has_value());   // empty cell inside surround
    CHECK(!takeoff.lookup(10.0, 2000.0).has_value());  // exactly on the empty cell
    CHECK(!takeoff.lookup(10.0, 3000.0).has_value());  // empty cell on one side
    CHECK(takeoff.lookup(10.0, 0.0).has_value());      // exact point next to empty cell
    CHECK_NEAR(takeoff.lookup(10.0, 0.0).value_or(-1), 94.0, 1e-9);
}

TEST_CASE("N1Table: malformed CSV is rejected") {
    CHECK(
        N1Table::loadFromCsv("oat_c\\pa_ft,0,2000\n-10,90.0,91.0\n0,92.0,93.0\n").has_value());
    CHECK(!N1Table::loadFromCsv("").has_value());
    CHECK(!N1Table::loadFromCsv("oat_c\\pa_ft,0,2000\n").has_value());  // no data rows
    CHECK(!N1Table::loadFromCsv("oat_c\\pa_ft,0,2000\n-10,90.0\n").has_value());  // ragged row
    CHECK(!N1Table::loadFromCsv("oat_c\\pa_ft,0,2000\n-10,abc,91.0\n")
               .has_value());  // garbage cell
    CHECK(!N1Table::loadFromCsv("oat_c\\pa_ft,2000,0\n-10,90.0,91.0\n")
               .has_value());  // PA descending
    CHECK(!N1Table::loadFromCsv("oat_c\\pa_ft,0,2000\n0,90.0,91.0\n-10,92.0,93.0\n")
               .has_value());  // OAT descending
}

// --- N1Computer ------------------------------------------------------------

TEST_CASE("power-up runs the 888 self-test, then shows N1") {
    const std::string fixturesDir = fixturesDirectory();
    N1Computer computer;
    CHECK(computer.loadTables(fixturesDir));

    Output out = computer.tick({.powered = false,
                                .onGround = true,
                                .ratC = 0.0,
                                .pressureAltFt = 1000.0,
                                .antiIce = 0,
                                .knobPressed = false},
                               0.1);
    CHECK(out.state == DisplayState::Off);

    out = run(computer, onGroundAt(), 4.9);
    CHECK(out.state == DisplayState::SelfTest888);
    out = run(computer, onGroundAt(), 0.3);
    CHECK(out.state == DisplayState::N1);
    CHECK_NEAR(out.value, 92.5, 1e-9);
    CHECK(out.mode == Mode::ToGa);
}

TEST_CASE("a power cycle restarts the self-test and clears the selected temp") {
    const std::string fixturesDir = fixturesDirectory();
    N1Computer computer = poweredOnGround(fixturesDir);
    run(computer, onGroundAt(), 1.0);
    computer.bumpTemp(5.0);
    CHECK(computer.selectedTempC().has_value());

    InputSnapshot dead = onGroundAt();
    dead.powered = false;
    Output out = computer.tick(dead, 0.1);
    CHECK(out.state == DisplayState::Off);
    CHECK(!computer.selectedTempC().has_value());

    out = run(computer, onGroundAt(), 1.0);
    CHECK(out.state == DisplayState::SelfTest888);
    out = run(computer, onGroundAt(), 4.5);
    CHECK(out.state == DisplayState::N1);
}

TEST_CASE("on the ground only TO/GA has a valid schedule") {
    const std::string fixturesDir = fixturesDirectory();
    N1Computer computer = poweredOnGround(fixturesDir);

    computer.setMode(Mode::Clb);
    Output out = run(computer, onGroundAt(), 0.2);
    CHECK(out.state == DisplayState::Dashes);
    CHECK(out.mode == Mode::Clb);

    computer.setMode(Mode::Cru);
    out = run(computer, onGroundAt(), 0.2);
    CHECK(out.state == DisplayState::Dashes);
    CHECK(out.mode == Mode::Cru);

    computer.setMode(Mode::ToGa);
    out = run(computer, onGroundAt(), 0.2);
    CHECK(out.state == DisplayState::N1);
    CHECK_NEAR(out.value, 92.5, 1e-9);
}

TEST_CASE("anti-ice selects the wet schedule; partial anti-ice dashes") {
    const std::string fixturesDir = fixturesDirectory();
    N1Computer computer = poweredOnGround(fixturesDir);

    InputSnapshot partial = onGroundAt();
    partial.antiIce = 1;
    CHECK(run(computer, partial, 0.2).state == DisplayState::Dashes);

    InputSnapshot allOn = onGroundAt();
    allOn.antiIce = 2;
    Output out = run(computer, allOn, 0.2);
    CHECK(out.state == DisplayState::N1);
    CHECK_NEAR(out.value, 87.5, 1e-9);  // anti-ice takeoff table

    InputSnapshot airPartial = airborneAt(-10.0, 20000.0);
    airPartial.antiIce = 1;
    computer.setMode(Mode::Clb);
    CHECK(run(computer, airPartial, 0.2).state == DisplayState::Dashes);

    InputSnapshot airAllOn = airborneAt(-10.0, 20000.0);
    airAllOn.antiIce = 2;
    out = run(computer, airAllOn, 0.2);
    CHECK(out.state == DisplayState::N1);
    CHECK_NEAR(out.value, 52.0, 1e-9);  // anti-ice climb table
}

TEST_CASE("a missing chart cell shows dashes") {
    const std::string fixturesDir = fixturesDirectory();
    N1Computer computer = poweredOnGround(fixturesDir);
    CHECK(run(computer, onGroundAt(5.0, 1000.0), 0.2).state == DisplayState::Dashes);
}

TEST_CASE("selected temperature: set, apply, and clear on liftoff") {
    const std::string fixturesDir = fixturesDirectory();
    N1Computer computer = poweredOnGround(fixturesDir);

    InputSnapshot pressed = onGroundAt(0.0, 0.0);
    pressed.knobPressed = true;
    Output out = run(computer, pressed, 0.2);
    CHECK(out.state == DisplayState::Rat);
    CHECK_NEAR(out.value, 0.0, 1e-9);

    computer.bumpTemp(2.0);
    out = run(computer, pressed, 0.2);
    CHECK(out.state == DisplayState::TempSet);
    CHECK_NEAR(out.value, 2.0, 1e-9);

    computer.bumpTemp(-1.0);
    out = run(computer, pressed, 0.2);
    CHECK(out.state == DisplayState::TempSet);
    CHECK_NEAR(out.value, 1.0, 1e-9);

    out = run(computer, onGroundAt(0.0, 0.0), 0.2);  // released in TO/GA
    CHECK(out.state == DisplayState::N1);
    CHECK_NEAR(out.value, 92.2, 1e-9);  // takeoff N1 at selected 1 degC, not RAT
    CHECK_NEAR(computer.selectedTempC().value_or(-999), 1.0, 1e-9);

    out = run(computer, pressed, 0.2);  // re-press without rotating shows RAT again
    CHECK(out.state == DisplayState::Rat);

    out = run(computer, airborneAt(0.0, 1000.0), 0.2);  // liftoff clears selected temp
    CHECK(!computer.selectedTempC().has_value());
    CHECK(out.state == DisplayState::N1);
    CHECK_NEAR(out.value, 82.2, 1e-9);  // go-around table at actual RAT
}

TEST_CASE("selected temperature saturates at the chart edges") {
    const std::string fixturesDir = fixturesDirectory();
    N1Computer computer = poweredOnGround(fixturesDir);
    run(computer, onGroundAt(), 0.2);
    for (int i = 0; i < 400; ++i) computer.bumpTemp(1.0);
    CHECK_NEAR(computer.selectedTempC().value_or(0.0), 99.0, 1e-9);
    for (int i = 0; i < 800; ++i) computer.bumpTemp(-1.0);
    CHECK_NEAR(computer.selectedTempC().value_or(0.0), -99.0, 1e-9);
}

TEST_CASE("selected temperature cannot be changed airborne") {
    const std::string fixturesDir = fixturesDirectory();
    N1Computer computer = poweredOnGround(fixturesDir);
    run(computer, airborneAt(), 0.2);
    computer.bumpTemp(5.0);
    CHECK(!computer.selectedTempC().has_value());

    InputSnapshot pressed = airborneAt(-7.0, 1000.0);
    pressed.knobPressed = true;
    Output out = run(computer, pressed, 0.2);
    CHECK(out.state == DisplayState::Rat);
    CHECK_NEAR(out.value, -7.0, 1e-9);
}

TEST_CASE("airborne CLB, CRU and go-around targets") {
    const std::string fixturesDir = fixturesDirectory();
    N1Computer computer = poweredOnGround(fixturesDir);

    computer.setMode(Mode::Clb);
    Output out = run(computer, airborneAt(-10.0, 20000.0), 0.2);
    CHECK(out.state == DisplayState::N1);
    CHECK_NEAR(out.value, 62.0, 1e-9);

    computer.setMode(Mode::Cru);
    out = run(computer, airborneAt(-10.0, 20000.0), 0.2);
    CHECK(out.state == DisplayState::N1);
    CHECK_NEAR(out.value, 42.0, 1e-9);
}

TEST_CASE("go-around is gated above 15,500 ft") {
    const std::string fixturesDir = fixturesDirectory();
    N1Computer computer = poweredOnGround(fixturesDir);
    run(computer, airborneAt(), 0.2);

    Output out = run(computer, airborneAt(-10.0, 15000.0), 0.2);
    CHECK(out.state == DisplayState::N1);
    CHECK_NEAR(out.value, 83.0, 1e-9);

    out = run(computer, airborneAt(-10.0, 15500.0), 0.2);  // at the gate: still valid
    CHECK(out.state == DisplayState::N1);

    out = run(computer, airborneAt(-10.0, 16000.0), 0.2);  // above the gate
    CHECK(out.state == DisplayState::Dashes);

    out = run(computer, airborneAt(-10.0, 15000.0), 0.2);  // descending restores it
    CHECK(out.state == DisplayState::N1);
}

TEST_CASE("the display reverts to 888 a minute after landing") {
    const std::string fixturesDir = fixturesDirectory();
    N1Computer computer = poweredOnGround(fixturesDir);
    run(computer, airborneAt(), 30.0);  // a flight, not a bounce

    Output out = run(computer, onGroundAt(), 59.8);  // touchdown, under a minute
    CHECK(out.state == DisplayState::N1);

    out = run(computer, onGroundAt(), 0.4);  // one minute after touchdown
    CHECK(out.state == DisplayState::SelfTest888);
    out = run(computer, onGroundAt(), 30.0);  // stays latched
    CHECK(out.state == DisplayState::SelfTest888);

    InputSnapshot pressed = onGroundAt();
    pressed.knobPressed = true;
    out = run(computer, pressed, 0.2);  // knob press wakes it
    CHECK(out.state == DisplayState::Rat);

    out = run(computer, onGroundAt(), 61.0);  // no re-latch without a new touchdown
    CHECK(out.state == DisplayState::N1);
}

TEST_CASE("a brief weight-on-wheels blip is not a landing") {
    const std::string fixturesDir = fixturesDirectory();
    N1Computer computer = poweredOnGround(fixturesDir);
    run(computer, airborneAt(), 0.5);  // the sim drops the aircraft onto its gear
    Output out = run(computer, onGroundAt(), 90.0);
    CHECK(out.state == DisplayState::N1);  // never armed, so no standby latch
}

TEST_CASE("a mode change wakes the display from 888") {
    const std::string fixturesDir = fixturesDirectory();
    N1Computer computer = poweredOnGround(fixturesDir);
    run(computer, airborneAt(), 30.0);
    Output out = run(computer, onGroundAt(), 61.0);
    CHECK(out.state == DisplayState::SelfTest888);

    computer.setMode(Mode::Clb);
    out = run(computer, onGroundAt(), 0.2);
    CHECK(out.state == DisplayState::Dashes);  // awake, showing the invalid-mode display
}

// --- Real data anchors (skipped when data/ CSVs are absent) ----------------

TEST_CASE("shipped tables: anchor values from the performance manual") {
    const std::string dataDir = dataDirectory();
    const auto takeoffCsv = readFile(dataDir + "/n1_takeoff.csv");
    if (!takeoffCsv) {
        std::printf("note: %s/n1_takeoff.csv absent; skipping real-data anchors\n",
                    dataDir.c_str());
        return;
    }
    const auto takeoff = N1Table::loadFromCsv(*takeoffCsv);
    CHECK(takeoff.has_value());
    // Chart grid points (perf manual, sea-level takeoff page): 95.8 @ 10degC,
    // 97.5 @ 20degC, 95.9 @ 30degC (the SIMCOM 11-30degC envelope minimum).
    if (takeoff) {
        CHECK_NEAR(takeoff->lookup(20.0, 0.0).value_or(-1), 97.5, 0.05);
        CHECK_NEAR(takeoff->lookup(30.0, 0.0).value_or(-1), 95.9, 0.05);
        CHECK_NEAR(takeoff->lookup(15.0, 0.0).value_or(-1), 96.65, 0.05);
    }

    const auto climbCsv = readFile(dataDir + "/n1_climb.csv");
    CHECK(climbCsv.has_value());
    if (!climbCsv) return;
    const auto climb = N1Table::loadFromCsv(*climbCsv);
    CHECK(climb.has_value());
    if (climb) CHECK_NEAR(climb->lookup(-45.0, 10000.0).value_or(-1), 100.7, 0.05);
}

// --- Failures --------------------------------------------------------------

TEST_CASE("an air data failure blanks the display but leaves it energised") {
    const std::string fixturesDir = fixturesDirectory();
    N1Computer computer = poweredOnGround(fixturesDir);

    CHECK(run(computer, onGroundAt(), 0.2).state == DisplayState::N1);

    InputSnapshot failed = onGroundAt();
    failed.airDataFailed = true;
    CHECK(run(computer, failed, 0.2).state == DisplayState::Fail);

    // A failure arising after power-up clears when the air data recovers.
    CHECK(run(computer, onGroundAt(), 0.2).state == DisplayState::N1);
}

TEST_CASE("an air data failure outranks the knob") {
    const std::string fixturesDir = fixturesDirectory();
    N1Computer computer = poweredOnGround(fixturesDir);

    InputSnapshot failedPressed = onGroundAt();
    failedPressed.airDataFailed = true;
    failedPressed.knobPressed = true;
    // RAT comes from the same air data source, so a press must not reveal it.
    CHECK(run(computer, failedPressed, 0.2).state == DisplayState::Fail);
}

TEST_CASE("a failure at power-up latches an unsatisfactory self-test") {
    const std::string fixturesDir = fixturesDirectory();
    N1Computer computer;
    CHECK(computer.loadTables(fixturesDir));

    InputSnapshot unpowered = onGroundAt();
    unpowered.powered = false;
    unpowered.airDataFailed = true;
    run(computer, unpowered, 0.5);

    // Powering up with bad air data is an unsatisfactory self-test: blank, no 888.
    InputSnapshot failedPowerUp = onGroundAt();
    failedPowerUp.airDataFailed = true;
    CHECK(run(computer, failedPowerUp, 0.2).state == DisplayState::Fail);
    CHECK(run(computer, failedPowerUp, 5.2).state == DisplayState::Fail);

    // The self-test result latches: recovering the air data is not enough.
    CHECK(run(computer, onGroundAt(), 1.0).state == DisplayState::Fail);

    // Only a power cycle re-runs the self-test.
    run(computer, unpowered, 0.5);
    CHECK(run(computer, onGroundAt(), 0.2).state == DisplayState::SelfTest888);
    CHECK(run(computer, onGroundAt(), 5.2).state == DisplayState::N1);
}

TEST_CASE("off-chart inputs dash rather than clamping") {
    const std::string fixturesDir = fixturesDirectory();
    N1Computer computer = poweredOnGround(fixturesDir);

    // Fixture takeoff axes: OAT -10..20, PA 0..4000.
    CHECK(run(computer, onGroundAt(0.0, 2000.0), 0.2).state == DisplayState::N1);
    CHECK(run(computer, onGroundAt(0.0, 9000.0), 0.2).state == DisplayState::Dashes);
    CHECK(run(computer, onGroundAt(0.0, -50.0), 0.2).state ==
          DisplayState::N1);  // high QNH at sea level
    CHECK(run(computer, onGroundAt(60.0, 2000.0), 0.2).state == DisplayState::Dashes);
    CHECK(run(computer, onGroundAt(-40.0, 2000.0), 0.2).state == DisplayState::Dashes);
}

}  // namespace
