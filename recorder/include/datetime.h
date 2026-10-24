#pragma once

#include <chrono>
#include <cstdint>
#include <string>

// The .NET DateTime binary format, used by the .kbi files:
//
//   Bits 00 - 61: the number of 100 nanosecond ticks since 0001-01-01
//   Bits 62 - 63: the DateTimeKind (0 = Unspecified, 1 = Utc, 2 = Local)
//
// The kind bits are not part of the time and have to be masked off when the
// value is converted.
namespace Kbi {

// The two upper bits of a .NET DateTime hold the DateTimeKind.
inline constexpr std::uint64_t kNetDateTimeKindMask = 0x3FFF'FFFF'FFFF'FFFFULL;

// The DateTimeKind values.
inline constexpr std::uint64_t kNetDateTimeKindUnspecified = 0;
inline constexpr std::uint64_t kNetDateTimeKindUtc = 1;
inline constexpr std::uint64_t kNetDateTimeKindLocal = 2;

// Converts a time point into .NET DateTime ticks.
[[nodiscard]] std::uint64_t toNetDatetimeTicks(
    std::chrono::system_clock::time_point timePoint, std::uint64_t kind = kNetDateTimeKindUtc);

// Converts .NET DateTime ticks into a time point. The DateTimeKind bits are
// ignored.
[[nodiscard]] std::chrono::system_clock::time_point fromNetDatetimeTicks(std::uint64_t ticks);

// Formats a time point as an ISO 8601 timestamp in UTC, for example
// "2026-10-21T20:30:45Z".
[[nodiscard]] std::string formatIso8601Utc(
    std::chrono::system_clock::time_point timePoint, bool milliseconds = false);

} // namespace Kbi
