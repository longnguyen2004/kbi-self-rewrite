#include <datetime.h>

#include <chrono>
#include <cstdint>
#include <format>
#include <ratio>
#include <string>

namespace Kbi {
namespace {

// 100 nanosecond intervals, the resolution of a .NET DateTime.
using net_ticks = std::chrono::duration<std::int64_t, std::ratio<1, 10'000'000>>;

// The number of days between the .NET epoch (0001-01-01) and the unix epoch.
constexpr std::int64_t kNetEpochDays = 719'162;

} // namespace

std::uint64_t toNetDatetimeTicks(
    std::chrono::system_clock::time_point timePoint, std::uint64_t kind)
{
    // The offset is converted step by step, its number of nanoseconds does not
    // fit into an int64.
    constexpr auto offsetTicks
        = std::chrono::duration_cast<net_ticks>(std::chrono::seconds(kNetEpochDays * 86'400));

    const auto ticks = std::chrono::duration_cast<net_ticks>(timePoint.time_since_epoch()).count()
        + offsetTicks.count();

    // The time itself is stored in the lower 62 bits, the two upper bits hold
    // the DateTimeKind.
    return (static_cast<std::uint64_t>(ticks) & kNetDateTimeKindMask) | ((kind & 3) << 62);
}

std::chrono::system_clock::time_point fromNetDatetimeTicks(std::uint64_t ticks)
{
    // The DateTimeKind bits are not part of the time.
    ticks &= kNetDateTimeKindMask;

    // The number of 100 nanosecond intervals between 0001-01-01 and the unix
    // epoch.
    constexpr std::int64_t kNetEpochTicks = 621'355'968'000'000'000LL;

    const std::int64_t unixTicks = ticks >= static_cast<std::uint64_t>(kNetEpochTicks)
        ? static_cast<std::int64_t>(ticks) - kNetEpochTicks
        : -static_cast<std::int64_t>(static_cast<std::uint64_t>(kNetEpochTicks) - ticks);

    return std::chrono::system_clock::time_point {
        std::chrono::duration_cast<std::chrono::system_clock::duration>(net_ticks(unixTicks))
    };
}

std::string formatIso8601Utc(std::chrono::system_clock::time_point timePoint, bool milliseconds)
{
    // A system_clock time point is formatted in UTC. The time is truncated to
    // the wanted precision, the formatter appends the fractional seconds.
    if (milliseconds) {
        const auto truncated = std::chrono::floor<std::chrono::milliseconds>(timePoint);
        return std::format("{:%Y-%m-%dT%H:%M:%S}Z", truncated);
    }

    const auto truncated = std::chrono::floor<std::chrono::seconds>(timePoint);
    return std::format("{:%Y-%m-%dT%H:%M:%S}Z", truncated);
}

} // namespace Kbi
