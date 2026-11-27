#include <tests/tests.h>

#include <datetime.h>
#include <chrono>
#include <cstdint>
#include <print>

namespace Kbi::Tests {
void testDatetime()
{
    std::println("datetime");

    // The .NET epoch is 0001-01-01, the unix epoch is 1970-01-01.
    const auto unixEpoch = std::chrono::system_clock::time_point {};
    check(Kbi::formatIso8601Utc(unixEpoch) == "1970-01-01T00:00:00Z",
        "the unix epoch is formatted correctly");

    // 2026-09-26T18:39:27.211Z, the time of the "SayoDevice 7K mechanical.kbi"
    // recording.
    constexpr std::uint64_t ticks = 639'260'447'672'114'184;
    const auto timePoint = Kbi::fromNetDatetimeTicks(ticks);
    check(Kbi::formatIso8601Utc(timePoint, true) == "2026-09-26T18:39:27.211Z",
        "the .NET ticks are converted correctly (" + Kbi::formatIso8601Utc(timePoint, true) + ")");

    // The two upper bits hold the DateTimeKind, they are not part of the time.
    const std::uint64_t withKind = ticks | (2ULL << 62);
    check(Kbi::fromNetDatetimeTicks(withKind) == timePoint, "the DateTimeKind bits are ignored");
    check(Kbi::fromNetDatetimeTicks(ticks | (1ULL << 62)) == timePoint, "the Utc kind is ignored");

    // The round trip through the .NET DateTime encoding, the same conversion
    // the .kbi exporter uses.
    const auto roundTrip
        = Kbi::fromNetDatetimeTicks(Kbi::toNetDatetimeTicks(timePoint, Kbi::kNetDateTimeKindUtc));
    check(roundTrip == timePoint, "the .NET DateTime encoding round trips");

    // The kind bits are set, but they do not change the time.
    check(Kbi::toNetDatetimeTicks(timePoint, Kbi::kNetDateTimeKindUtc)
            != Kbi::toNetDatetimeTicks(timePoint, Kbi::kNetDateTimeKindLocal),
        "the DateTimeKind is stored in the upper bits");
    check((Kbi::toNetDatetimeTicks(timePoint, Kbi::kNetDateTimeKindUtc) >> 62) == 1,
        "the Utc kind is written");

    // A time before the unix epoch.
    const auto before = Kbi::fromNetDatetimeTicks(621'355'968'000'000'000ULL - 1);
    check(Kbi::formatIso8601Utc(before) == "1969-12-31T23:59:59Z",
        "times before the unix epoch are converted correctly (" + Kbi::formatIso8601Utc(before)
            + ")");
}

} // namespace Kbi::Tests
