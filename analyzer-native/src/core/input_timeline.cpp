#include "core/input_timeline.h"

#include "core/keycode_util.h"

#include <algorithm>
#include <format>

namespace kbi {

void InputTimeline::reset() {
    m_keypresses.clear();
    m_keys.clear();
    m_key_down_map.clear();
    m_end_timestamp = 0.0;
}

void InputTimeline::add(const std::string& device_id, const InputEvent& event) {
    const std::string map_key = std::format("{}-{}", device_id, event.code);
    if (event.pressed) {
        m_key_down_map[map_key] = event.timestamp;
        return;
    }

    const auto it = m_key_down_map.find(map_key);
    if (it == m_key_down_map.end())
        return;
    const std::uint64_t keydown_time = it->second;
    m_key_down_map.erase(it);

    const std::string key_name(keycode_name(event.code));
    const double start = static_cast<double>(keydown_time) / 1000000.0;
    const double end = static_cast<double>(event.timestamp) / 1000000.0;
    m_keypresses.push_back(Keypress{start, end, key_name, device_id});
    if (std::find(m_keys.begin(), m_keys.end(), key_name) == m_keys.end())
        m_keys.push_back(key_name);
    m_end_timestamp = std::max(m_end_timestamp, end);
}

} // namespace kbi
