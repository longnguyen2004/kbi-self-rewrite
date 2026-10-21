#pragma once

#include "core/result.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace kbi {

// A press/release pair turned into an interval.
struct Keypress {
    double start = 0.0; // seconds
    double end = 0.0;   // seconds
    std::string key;    // keycode name
    std::string device_id;
};

// Turns press/release event pairs into keypress intervals.
class InputTimeline {
public:
    void reset();
    void add(const std::string& device_id, const InputEvent& event);

    [[nodiscard]] const std::vector<Keypress>& keypresses() const { return m_keypresses; }
    // The keys in the order they were first pressed.
    [[nodiscard]] const std::vector<std::string>& keys() const { return m_keys; }
    [[nodiscard]] double end_timestamp() const { return m_end_timestamp; }

private:
    std::vector<Keypress> m_keypresses;
    std::vector<std::string> m_keys;
    double m_end_timestamp = 0.0;
    std::map<std::string, double> m_key_down_map;
};

} // namespace kbi
