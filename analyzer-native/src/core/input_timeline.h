#pragma once

#include <core/result.h>

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace Kbi {

// A press/release pair turned into an interval.
struct Keypress {
    double start = 0.0; // seconds
    double end = 0.0; // seconds
    std::string key; // keycode name
    std::string deviceId;
};

// Turns press/release event pairs into keypress intervals.
class InputTimeline {
public:
    void reset();
    void add(const std::string& deviceId, const InputEvent& event);

    [[nodiscard]] const std::vector<Keypress>& keypresses() const { return m_keypresses; }
    // The keys in the order they were first pressed.
    [[nodiscard]] const std::vector<std::string>& keys() const { return m_keys; }
    [[nodiscard]] double endTimestamp() const { return m_endTimestamp; }

private:
    std::vector<Keypress> m_keypresses;
    std::vector<std::string> m_keys;
    double m_endTimestamp = 0.0;
    std::map<std::string, double> m_keyDownMap;
};

} // namespace Kbi
