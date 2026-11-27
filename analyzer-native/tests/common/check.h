#pragma once

#include <print>
#include <string>

namespace Kbi::Tests {

// The failure count lives in a function-local static instead of a
// namespace-scope variable (I.2, R.6).
inline int& failures()
{
    static int count = 0;
    return count;
}

inline void check(bool condition, const std::string& what)
{
    if (condition) {
        std::println("  ok   {}", what);
    } else {
        std::println("  FAIL {}", what);
        ++failures();
    }
}

} // namespace Kbi::Tests
